/*
 * settings.c — Camera settings state machine and OV7670 runtime controls
 *
 * Implements the Auto/S/M shooting mode state machine with live
 * OV7670 register adjustments for exposure, gain, brightness, etc.
 */
#include "settings.h"
#include "ov7670_init.h"
#include "i2c.h"
#include "uart.h"
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
#define REG_AEW     0x24
#define REG_AEB     0x25
#define REG_VPT     0x26
#define REG_COM11   0x3B
#define REG_BRIGHT  0x55
#define REG_CONTRAS 0x56
#define REG_DBLV    0x6B
#define REG_ADVFL   0x9D
#define REG_ADVFH   0x9E

/* Global settings */
camera_settings_t cam_settings;

/* Shutter speed tables: CLKRC values only (don't touch DBLV/PLL)
 * Empirical: CLKRC prescaler divides by 2*(N+1) on this OV7670.
 *   0x80 = bypass → ~30fps
 *   0x00 = ÷2     → ~16fps
 *   0x01 = ÷4     → ~8fps
 */
static const int shutter_clkrc[SHUTTER_COUNT] = { 0x80, 0x00, 0x01, 0x03 };

/* Exposure AEC line counts (Manual mode only)
 * AECH register = lines >> 2, so we store the AECH value directly.
 *   480 lines = 0x78, 240 = 0x3C, 120 = 0x1E, 60 = 0x0F
 */
static const int exposure_aech[EXPOSURE_COUNT] = { 0x78, 0x3C, 0x1E, 0x0F };

/* ISO gain register values */
static const int iso_gain[ISO_COUNT] = { 0x00, 0x10, 0x30, 0x70, 0xF0, 0xFF };

/* ISO AGC ceiling (COM9 values) for auto modes */
static const int iso_ceiling[ISO_COUNT] = { 0x08, 0x18, 0x18, 0x38, 0x38, 0x48 };

/* EV compensation: AEW/AEB pairs indexed by (ev_comp - EV_MIN) */
static const int ev_aew[] = { 0x35, 0x45, 0x55, 0x65, 0x75, 0x85, 0x95, 0xA5, 0xC0 };
static const int ev_aeb[] = { 0x15, 0x1D, 0x25, 0x2C, 0x33, 0x44, 0x55, 0x65, 0x75 };

/* Night mode COM11 values */
static const int night_com11[] = { 0x0A, 0x2A, 0x4A, 0xEA };

/* Helper: write OV7670 register (short form) */
static void ov_wr(int reg, int val)
{
    i2c_write(OV7670_ADDR, reg, val);
}

void settings_init(void)
{
    cam_settings.shoot_mode = SHOOT_AUTO;
    cam_settings.shutter = SHUTTER_FAST;
    cam_settings.exposure = EXPOSURE_FULL;
    cam_settings.iso = ISO_800;
    cam_settings.ev_comp = 0;
    cam_settings.brightness = 0;
    cam_settings.contrast = 0x40;
    cam_settings.night_mode = NIGHT_EIGHTH;
    cam_settings.mirror = 0;
    cam_settings.flip = 0;
    cam_settings.show_hud = 1;

    /* Apply all settings to sensor */
    settings_apply_mode();
    settings_apply_brightness();
    settings_apply_contrast();
    settings_apply_orientation();
}

void settings_apply_mode(void)
{
    switch (cam_settings.shoot_mode) {
    case SHOOT_AUTO:
        /* Full sensor re-init for auto mode */
        ov7670_reset_auto();
        settings_apply_ev();
        uart_puts("[Auto]\n");
        break;

    case SHOOT_M:
        /* Proper manual mode transition via sensor driver */
        ov7670_set_manual();
        settings_apply_shutter();
        settings_apply_exposure();
        settings_apply_iso();
        uart_puts("[Manual]\n");
        break;
    }
}

void settings_apply_shutter(void)
{
    int idx;
    idx = cam_settings.shutter;
    if (idx < 0) idx = 0;
    if (idx >= SHUTTER_COUNT) idx = SHUTTER_COUNT - 1;

    /* Set CLKRC to control frame rate */
    ov_wr(REG_CLKRC, shutter_clkrc[idx]);

    /* In manual mode, also apply the current exposure preset */
    if (cam_settings.shoot_mode == SHOOT_M) {
        settings_apply_exposure();
    }
}

void settings_apply_exposure(void)
{
    int idx;
    idx = cam_settings.exposure;
    if (idx < 0) idx = 0;
    if (idx >= EXPOSURE_COUNT) idx = EXPOSURE_COUNT - 1;

    ov_wr(REG_AECHH, 0x00);               /* AEC[15:10] = 0 */
    ov_wr(REG_AECH,  exposure_aech[idx]);  /* AEC[9:2] */
    ov_wr(REG_COM1,  0x00);               /* AEC[1:0] = 0 */
}

void settings_apply_iso(void)
{
    int idx;
    idx = cam_settings.iso;
    if (idx < 0) idx = 0;
    if (idx >= ISO_COUNT) idx = ISO_COUNT - 1;

    if (cam_settings.shoot_mode == SHOOT_M) {
        /* Manual: directly set gain register */
        ov_wr(REG_GAIN, iso_gain[idx]);
    } else {
        /* Auto/S: set AGC ceiling */
        ov_wr(REG_COM9, iso_ceiling[idx]);
    }
}

void settings_apply_ev(void)
{
    int idx;
    /* Only meaningful in Auto/S modes */
    if (cam_settings.shoot_mode == SHOOT_M) return;

    idx = cam_settings.ev_comp - EV_MIN;
    if (idx < 0) idx = 0;
    if (idx > (EV_MAX - EV_MIN)) idx = EV_MAX - EV_MIN;

    ov_wr(REG_AEW, ev_aew[idx]);
    ov_wr(REG_AEB, ev_aeb[idx]);
}

void settings_apply_brightness(void)
{
    int val;
    val = cam_settings.brightness;

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
    val = cam_settings.contrast;
    if (val < 0) val = 0;
    if (val > 127) val = 127;
    ov_wr(REG_CONTRAS, val);
}

void settings_apply_night(void)
{
    int idx;
    idx = cam_settings.night_mode;
    if (idx < 0) idx = 0;
    if (idx > 3) idx = 3;
    ov_wr(REG_COM11, night_com11[idx]);
}

void settings_apply_orientation(void)
{
    int val;
    val = 0x00;
    if (cam_settings.mirror) val = val | 0x20;
    if (cam_settings.flip) val = val | 0x10;
    ov_wr(REG_MVFP, val);
}

void settings_cycle_mode(void)
{
    if (cam_settings.shoot_mode == SHOOT_AUTO)
        cam_settings.shoot_mode = SHOOT_M;
    else
        cam_settings.shoot_mode = SHOOT_AUTO;
    settings_apply_mode();
}

void settings_reset(void)
{
    settings_init();
    uart_puts("Settings reset\n");
}

void settings_adjust_shutter(int direction)
{
    /* Only adjustable in Manual mode */
    if (cam_settings.shoot_mode != SHOOT_M) return;

    cam_settings.shutter = cam_settings.shutter + direction;
    if (cam_settings.shutter < 0) cam_settings.shutter = 0;
    if (cam_settings.shutter >= SHUTTER_COUNT) cam_settings.shutter = SHUTTER_COUNT - 1;
    /* Caller must call settings_apply_shutter() with camera stopped */
}

void settings_adjust_iso(int direction)
{
    /* Only adjustable in Manual mode */
    if (cam_settings.shoot_mode != SHOOT_M) return;

    cam_settings.iso = cam_settings.iso + direction;
    if (cam_settings.iso < 0) cam_settings.iso = 0;
    if (cam_settings.iso >= ISO_COUNT) cam_settings.iso = ISO_COUNT - 1;
    /* Caller must call settings_apply_iso() with camera stopped */
}

void settings_adjust_exposure(int direction)
{
    /* Only adjustable in Manual mode */
    if (cam_settings.shoot_mode != SHOOT_M) return;

    cam_settings.exposure = cam_settings.exposure + direction;
    if (cam_settings.exposure < 0) cam_settings.exposure = 0;
    if (cam_settings.exposure >= EXPOSURE_COUNT) cam_settings.exposure = EXPOSURE_COUNT - 1;
    /* Caller must call settings_apply_exposure() with camera stopped */
}

void settings_adjust_ev(int direction)
{
    cam_settings.ev_comp = cam_settings.ev_comp + direction;
    if (cam_settings.ev_comp < EV_MIN) cam_settings.ev_comp = EV_MIN;
    if (cam_settings.ev_comp > EV_MAX) cam_settings.ev_comp = EV_MAX;
    settings_apply_ev();
}

void settings_adjust_brightness(int direction)
{
    cam_settings.brightness = cam_settings.brightness + (direction * 16);
    if (cam_settings.brightness < -128) cam_settings.brightness = -128;
    if (cam_settings.brightness > 127) cam_settings.brightness = 127;
    settings_apply_brightness();
}

void settings_adjust_contrast(int direction)
{
    cam_settings.contrast = cam_settings.contrast + (direction * 8);
    if (cam_settings.contrast < 0) cam_settings.contrast = 0;
    if (cam_settings.contrast > 127) cam_settings.contrast = 127;
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

const char *settings_mode_str(void)
{
    switch (cam_settings.shoot_mode) {
    case SHOOT_AUTO: return "A";
    case SHOOT_M:    return "M";
    default:         return "?";
    }
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
    case EXPOSURE_EIGHTH:  return "1/8";
    default:               return "?";
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
