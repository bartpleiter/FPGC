/*
 * settings.h — Camera settings and OV7670 runtime controls
 *
 * Manual-only shooting with runtime OV7670 register adjustments
 * for shutter, exposure, gain, brightness, contrast, and more.
 */
#ifndef SETTINGS_H
#define SETTINGS_H

/* Shutter speed presets (frame rate via CLKRC prescaler) */
#define SHUTTER_FAST    0   /* ~30 fps */
#define SHUTTER_NORMAL  1   /* ~16 fps */
#define SHUTTER_SLOW    2   /* ~8 fps */
#define SHUTTER_SLOWER  3   /* ~4 fps */
#define SHUTTER_COUNT   4

/* Exposure presets (AEC lines) */
#define EXPOSURE_FULL    0   /* 480 lines — max light */
#define EXPOSURE_HALF    1   /* 240 lines */
#define EXPOSURE_QUARTER 2   /* 120 lines */
#define EXPOSURE_EIGHTH    3   /* 60 lines — freeze motion */
#define EXPOSURE_SIXTEENTH 4   /* 30 lines — extreme freeze */
#define EXPOSURE_COUNT     5

/* ISO presets (direct gain register values) */
#define ISO_100   0
#define ISO_200   1
#define ISO_400   2
#define ISO_800   3
#define ISO_1600  4
#define ISO_3200  5
#define ISO_COUNT 6

/* Sharpness presets */
#define SHARPNESS_OFF    0
#define SHARPNESS_LOW    1
#define SHARPNESS_MEDIUM 2
#define SHARPNESS_HIGH   3
#define SHARPNESS_COUNT  4

/* Gamma presets */
#define GAMMA_LINEAR     0
#define GAMMA_STANDARD   1
#define GAMMA_HICONTRAST 2
#define GAMMA_LOWLIGHT   3
#define GAMMA_COUNT      4

/* Per-display-mode brightness/contrast offsets */
typedef struct {
    int brightness;  /* offset -8 to +8 */
    int contrast;    /* offset -8 to +8 */
} mode_preset_t;

/* Camera settings state */
typedef struct {
    int shutter;          /* SHUTTER_FAST / NORMAL / SLOW / SLOWER */
    int exposure;         /* EXPOSURE_FULL .. EXPOSURE_SIXTEENTH */
    int iso;              /* ISO_100 .. ISO_3200 */
    int brightness;       /* offset -8 to +8 (0 = neutral) */
    int contrast;         /* offset -8 to +8 (0 = neutral) */
    int mirror;           /* 0 or 1 */
    int flip;             /* 0 or 1 */
    int show_hud;         /* 0 or 1 */
    int sharpness;        /* SHARPNESS_OFF .. SHARPNESS_HIGH */
    int gamma_preset;     /* GAMMA_LINEAR .. GAMMA_LOWLIGHT */
    mode_preset_t mode_presets[3]; /* per-display-mode B/C */
    int res_mode;         /* RES_QVGA / RES_QQVGA */
    int display_mode;     /* MODE_RAW / MODE_DITHER / MODE_DITHER8 */
} camera_settings_t;

/* Global settings instance */
extern camera_settings_t cam_settings;

/* Initialize settings to defaults and apply to sensor */
void settings_init(void);

/* Re-apply current settings after a resolution switch.
 * The sensor was just re-init'd by ov7670_set_q(q)vga(), so we
 * only write the overlay registers here. */
void settings_reapply(void);

/* Apply shutter speed preset (changes CLKRC) */
void settings_apply_shutter(void);

/* Apply exposure preset (AEC lines) */
void settings_apply_exposure(void);

/* Apply ISO/gain settings */
void settings_apply_iso(void);

/* Apply brightness register */
void settings_apply_brightness(void);

/* Apply contrast register */
void settings_apply_contrast(void);

/* Apply mirror/flip */
void settings_apply_orientation(void);

/* Apply sharpness preset to sensor */
void settings_apply_sharpness(void);

/* Apply gamma preset to sensor */
void settings_apply_gamma(void);

/* Reset all settings to defaults and re-apply to sensor */
void settings_reset(void);

/* Adjust shutter speed: direction = +1 (faster) or -1 (slower) */
void settings_adjust_shutter(int direction);

/* Adjust exposure: direction = +1 (longer) or -1 (shorter) */
void settings_adjust_exposure(int direction);

/* Adjust ISO: direction = +1 (higher) or -1 (lower) */
void settings_adjust_iso(int direction);

/* Adjust brightness: direction = +1 or -1 (offset steps) */
void settings_adjust_brightness(int direction);

/* Adjust contrast: direction = +1 or -1 (offset steps) */
void settings_adjust_contrast(int direction);

/* Toggle mirror */
void settings_toggle_mirror(void);

/* Toggle flip */
void settings_toggle_flip(void);

/* Toggle HUD display */
void settings_toggle_hud(void);

/* Adjust sharpness: direction = +1 or -1 */
void settings_adjust_sharpness(int direction);

/* Adjust gamma preset: direction = +1 or -1 */
void settings_adjust_gamma(int direction);

/* Preset save/load (3 slots: 0, 1, 2) */
#define PRESET_COUNT 3
int settings_save_preset(int slot);
int settings_load_preset(int slot);

/* Get human-readable strings for HUD display */
const char *settings_shutter_str(void);
const char *settings_exposure_str(void);
const char *settings_iso_str(void);
const char *settings_sharpness_str(void);
const char *settings_gamma_str(void);

#endif /* SETTINGS_H */
