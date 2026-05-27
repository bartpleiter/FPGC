/*
 * settings.c — Camera settings and OV7670 runtime controls
 *
 * Manual-only shooting with live OV7670 register adjustments for
 * shutter, exposure, gain, brightness, contrast, sharpness, gamma.
 */
#include "settings.h"
#include "ov7670_init.h"
#include "i2c.h"
#include "fpgc.h"
#include "cam_driver.h"

/* OV7670 register addresses */
#define REG_GAIN    0x00
#define REG_COM1    0x04
#define REG_AECHH   0x07
#define REG_AECH    0x10
#define REG_CLKRC   0x11
#define REG_COM8    0x13
#define REG_COM9    0x14
#define REG_MVFP    0x1E
#define REG_COM11   0x3B
#define REG_BRIGHT  0x55
#define REG_CONTRAS 0x56

/* OV7670 brightness/contrast mapping constants */
#define BRIGHT_STEP      16   /* each offset step = ±16 in register */
#define CONTRAST_DEFAULT 0x40 /* OV7670 neutral contrast register */
#define CONTRAST_STEP    8    /* each offset step = ±8 in register */

/* Sharpness registers */
#define REG_EDGE    0x3F
#define REG75       0x75
#define REG76       0x76

/* Gamma registers */
#define REG_SLOP    0x7A
#define REG_GAM1    0x7B
#define REG_COM13   0x3D

/* Global settings */
camera_settings_t cam_settings;

/* Shutter speed tables: CLKRC values
 *   0x80 = bypass → ~30fps
 *   0x00 = ÷2     → ~16fps
 *   0x01 = ÷4     → ~8fps
 */
static const int shutter_clkrc[SHUTTER_COUNT] = { 0x80, 0x00, 0x01, 0x03 };

/* Exposure AEC line counts: AECH register values */
static const int exposure_aech[EXPOSURE_COUNT] = { 0x78, 0x3C, 0x1E, 0x0F, 0x07 };

/* ISO gain register values */
static const int iso_gain[ISO_COUNT] = { 0x00, 0x10, 0x30, 0x70, 0xF0, 0xFF };

/* Sharpness preset tables: {EDGE, REG75, REG76} */
static const int sharp_edge[SHARPNESS_COUNT]  = { 0x00, 0x02, 0x06, 0x0F };
static const int sharp_reg75[SHARPNESS_COUNT] = { 0x00, 0x03, 0x05, 0x08 };
static const int sharp_reg76[SHARPNESS_COUNT] = { 0x00, 0x63, 0xA5, 0xE5 };

/* Gamma preset tables — 16 values per preset: SLOP + GAM1..GAM15 */
static const int gamma_standard[16] = {
    0x20, 0x10, 0x1E, 0x35, 0x5A, 0x69, 0x76, 0x80,
    0x88, 0x8F, 0x96, 0xA3, 0xAF, 0xC4, 0xD7, 0xE8
};
static const int gamma_hicontrast[16] = {
    0x10, 0x20, 0x38, 0x50, 0x68, 0x78, 0x84, 0x8E,
    0x96, 0x9C, 0xA0, 0xA8, 0xB0, 0xC0, 0xD0, 0xE0
};
static const int gamma_lowlight[16] = {
    0x08, 0x30, 0x48, 0x60, 0x74, 0x82, 0x8C, 0x94,
    0x9A, 0xA0, 0xA4, 0xAC, 0xB4, 0xC4, 0xD4, 0xE4
};

/* Helper: write OV7670 register (short form) */
static void ov_wr(int reg, int val)
{
    i2c_write(OV7670_ADDR, reg, val);
}

void settings_init(void)
{
    cam_settings.shutter = SHUTTER_FAST;
    cam_settings.exposure = EXPOSURE_FULL;
    cam_settings.iso = ISO_100;
    cam_settings.brightness = 0;
    cam_settings.contrast = 0;
    cam_settings.mirror = 0;
    cam_settings.flip = 0;
    cam_settings.show_hud = 1;
    cam_settings.sharpness = SHARPNESS_LOW;
    cam_settings.gamma_preset = GAMMA_STANDARD;

    /* Per-mode brightness/contrast defaults */
    cam_settings.mode_presets[0].brightness = 0;  /* RAW */
    cam_settings.mode_presets[0].contrast = 0;
    cam_settings.mode_presets[1].brightness = 0;  /* DITH */
    cam_settings.mode_presets[1].contrast = 0;
    cam_settings.mode_presets[2].brightness = 2;  /* DITH8 */
    cam_settings.mode_presets[2].contrast = 2;

    /* Apply all settings to sensor */
    settings_apply_shutter();
    settings_apply_exposure();
    settings_apply_iso();
    settings_apply_brightness();
    settings_apply_contrast();
    settings_apply_orientation();
    settings_apply_sharpness();
    settings_apply_gamma();
}

void settings_reapply(void)
{
    /* Re-apply settings after a resolution switch.
     * The sensor was just fully re-init'd by ov7670_set_q(q)vga(),
     * which leaves it in manual-ready state (AEC/AGC off, night off).
     * We write all overlay registers here. */
    settings_apply_shutter();
    settings_apply_exposure();
    settings_apply_iso();
    settings_apply_brightness();
    settings_apply_contrast();
    settings_apply_orientation();
    settings_apply_sharpness();
    settings_apply_gamma();
}

void settings_apply_shutter(void)
{
    int idx;
    idx = cam_settings.shutter;
    if (idx < 0) idx = 0;
    if (idx >= SHUTTER_COUNT) idx = SHUTTER_COUNT - 1;

    ov_wr(REG_CLKRC, shutter_clkrc[idx]);
    settings_apply_exposure();
}

void settings_apply_exposure(void)
{
    int idx;
    idx = cam_settings.exposure;
    if (idx < 0) idx = 0;
    if (idx >= EXPOSURE_COUNT) idx = EXPOSURE_COUNT - 1;

    ov_wr(REG_AECHH, 0x00);
    ov_wr(REG_AECH,  exposure_aech[idx]);
    ov_wr(REG_COM1,  0x00);
}

void settings_apply_iso(void)
{
    int idx;
    idx = cam_settings.iso;
    if (idx < 0) idx = 0;
    if (idx >= ISO_COUNT) idx = ISO_COUNT - 1;

    ov_wr(REG_GAIN, iso_gain[idx]);
}

void settings_apply_brightness(void)
{
    int val;
    val = cam_settings.brightness * BRIGHT_STEP;

    /* OV7670 uses sign-magnitude: bit7=sign, [6:0]=magnitude */
    if (val >= 0) {
        ov_wr(REG_BRIGHT, val & 0x7F);
    } else {
        ov_wr(REG_BRIGHT, 0x80 | ((-val) & 0x7F));
    }
}

void settings_apply_contrast(void)
{
    int val;
    val = CONTRAST_DEFAULT + cam_settings.contrast * CONTRAST_STEP;
    if (val < 0) val = 0;
    if (val > 127) val = 127;
    ov_wr(REG_CONTRAS, val);
}

void settings_apply_orientation(void)
{
    int val;
    val = 0x00;
    if (cam_settings.mirror) val = val | 0x20;
    if (cam_settings.flip) val = val | 0x10;
    ov_wr(REG_MVFP, val);
}

void settings_reset(void)
{
    settings_init();
}

void settings_adjust_shutter(int direction)
{
    cam_settings.shutter = cam_settings.shutter + direction;
    if (cam_settings.shutter < 0) cam_settings.shutter = 0;
    if (cam_settings.shutter >= SHUTTER_COUNT) cam_settings.shutter = SHUTTER_COUNT - 1;
    /* Caller should call settings_apply_shutter() with camera stopped (convention) */
}

void settings_adjust_iso(int direction)
{
    cam_settings.iso = cam_settings.iso + direction;
    if (cam_settings.iso < 0) cam_settings.iso = 0;
    if (cam_settings.iso >= ISO_COUNT) cam_settings.iso = ISO_COUNT - 1;
    /* Caller should call settings_apply_iso() with camera stopped (convention) */
}

void settings_adjust_exposure(int direction)
{
    cam_settings.exposure = cam_settings.exposure + direction;
    if (cam_settings.exposure < 0) cam_settings.exposure = 0;
    if (cam_settings.exposure >= EXPOSURE_COUNT) cam_settings.exposure = EXPOSURE_COUNT - 1;
    /* Caller should call settings_apply_exposure() with camera stopped (convention) */
}

void settings_adjust_brightness(int direction)
{
    cam_settings.brightness = cam_settings.brightness + direction;
    if (cam_settings.brightness < -8) cam_settings.brightness = -8;
    if (cam_settings.brightness > 8) cam_settings.brightness = 8;
    settings_apply_brightness();
}

void settings_adjust_contrast(int direction)
{
    cam_settings.contrast = cam_settings.contrast + direction;
    if (cam_settings.contrast < -8) cam_settings.contrast = -8;
    if (cam_settings.contrast > 8) cam_settings.contrast = 8;
    settings_apply_contrast();
}

void settings_toggle_mirror(void)
{
    cam_settings.mirror = !cam_settings.mirror;
    settings_apply_orientation();
}

void settings_toggle_flip(void)
{
    cam_settings.flip = !cam_settings.flip;
    settings_apply_orientation();
}

void settings_toggle_hud(void)
{
    cam_settings.show_hud = !cam_settings.show_hud;
}

void settings_apply_sharpness(void)
{
    int idx;
    idx = cam_settings.sharpness;
    if (idx < 0) idx = 0;
    if (idx >= SHARPNESS_COUNT) idx = SHARPNESS_COUNT - 1;
    ov_wr(REG_EDGE, sharp_edge[idx]);
    ov_wr(REG75, sharp_reg75[idx]);
    ov_wr(REG76, sharp_reg76[idx]);
}

void settings_apply_gamma(void)
{
    int idx;
    const int *g;
    int i;
    int com13;

    idx = cam_settings.gamma_preset;
    if (idx < 0) idx = 0;
    if (idx >= GAMMA_COUNT) idx = GAMMA_COUNT - 1;

    if (idx == GAMMA_LINEAR) {
        /* Disable gamma correction: clear COM13 bit 7 */
        com13 = 0x40;  /* UV sat auto on, gamma off */
        ov_wr(REG_COM13, com13);
        return;
    }

    /* Enable gamma correction: set COM13 bit 7 */
    ov_wr(REG_COM13, 0xC0);  /* gamma enable + UV sat auto */

    /* Select gamma curve table */
    if (idx == GAMMA_HICONTRAST)
        g = gamma_hicontrast;
    else if (idx == GAMMA_LOWLIGHT)
        g = gamma_lowlight;
    else
        g = gamma_standard;

    /* Write SLOP (0x7A) + GAM1..GAM15 (0x7B..0x89) */
    ov_wr(REG_SLOP, g[0]);
    for (i = 1; i < 16; i++) {
        ov_wr(REG_GAM1 + (i - 1), g[i]);
    }
}

void settings_adjust_sharpness(int direction)
{
    cam_settings.sharpness = cam_settings.sharpness + direction;
    if (cam_settings.sharpness < 0) cam_settings.sharpness = 0;
    if (cam_settings.sharpness >= SHARPNESS_COUNT)
        cam_settings.sharpness = SHARPNESS_COUNT - 1;
}

void settings_adjust_gamma(int direction)
{
    cam_settings.gamma_preset = cam_settings.gamma_preset + direction;
    if (cam_settings.gamma_preset < 0) cam_settings.gamma_preset = 0;
    if (cam_settings.gamma_preset >= GAMMA_COUNT)
        cam_settings.gamma_preset = GAMMA_COUNT - 1;
}

const char *settings_shutter_str(void)
{
    switch (cam_settings.shutter) {
    case SHUTTER_FAST:   return "1/30";
    case SHUTTER_NORMAL: return "1/16";
    case SHUTTER_SLOW:   return "1/8";
    case SHUTTER_SLOWER: return "1/4";
    default:             return "?";
    }
}

const char *settings_exposure_str(void)
{
    switch (cam_settings.exposure) {
    case EXPOSURE_FULL:    return "Full";
    case EXPOSURE_HALF:    return "1/2";
    case EXPOSURE_QUARTER: return "1/4";
    case EXPOSURE_EIGHTH:    return "1/8";
    case EXPOSURE_SIXTEENTH: return "1/16";
    default:                 return "?";
    }
}

const char *settings_iso_str(void)
{
    switch (cam_settings.iso) {
    case ISO_100:  return "100";
    case ISO_200:  return "200";
    case ISO_400:  return "400";
    case ISO_800:  return "800";
    case ISO_1600: return "1600";
    case ISO_3200: return "3200";
    default:       return "?";
    }
}

const char *settings_sharpness_str(void)
{
    switch (cam_settings.sharpness) {
    case SHARPNESS_OFF:    return "Off";
    case SHARPNESS_LOW:    return "Lo";
    case SHARPNESS_MEDIUM: return "Md";
    case SHARPNESS_HIGH:   return "Hi";
    default:               return "?";
    }
}

const char *settings_gamma_str(void)
{
    switch (cam_settings.gamma_preset) {
    case GAMMA_LINEAR:     return "Lin";
    case GAMMA_STANDARD:   return "Std";
    case GAMMA_HICONTRAST: return "HiC";
    case GAMMA_LOWLIGHT:   return "LoL";
    default:               return "?";
    }
}

/* ---- Preset save/load via BRFS ---- */
#include "storage.h"
#include "string.h"

static const char *preset_paths[PRESET_COUNT] = {
    "/preset_1", "/preset_2", "/preset_3"
};

int settings_save_preset(int slot)
{
    int fd;
    int rc;

    if (slot < 0 || slot >= PRESET_COUNT) return -1;
    if (!storage_ready) return -1;

    fd = brfs_open(&cam_brfs, preset_paths[slot]);
    if (fd < 0) {
        rc = brfs_create_file(&cam_brfs, preset_paths[slot]);
        if (rc != 0) return -1;
        fd = brfs_open(&cam_brfs, preset_paths[slot]);
        if (fd < 0) return -1;
    }
    brfs_truncate(&cam_brfs, fd);
    brfs_seek(&cam_brfs, fd, 0);
    rc = brfs_write(&cam_brfs, fd,
                    (const char *)&cam_settings,
                    (unsigned int)sizeof(camera_settings_t));
    brfs_close(&cam_brfs, fd);
    storage_sync();
    return (rc > 0) ? 0 : -1;
}

int settings_load_preset(int slot)
{
    int fd;
    int rc;
    camera_settings_t tmp;

    if (slot < 0 || slot >= PRESET_COUNT) return -1;
    if (!storage_ready) return -1;

    fd = brfs_open(&cam_brfs, preset_paths[slot]);
    if (fd < 0) return -1;

    rc = brfs_read(&cam_brfs, fd,
                   (char *)&tmp,
                   (unsigned int)sizeof(camera_settings_t));
    brfs_close(&cam_brfs, fd);

    if (rc < (int)sizeof(camera_settings_t)) return -1;

    /* Apply loaded settings */
    memcpy(&cam_settings, &tmp, sizeof(camera_settings_t));
    settings_reapply();
    return 0;
}
