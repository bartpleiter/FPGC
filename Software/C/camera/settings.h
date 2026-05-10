/*
 * settings.h — Camera settings state machine and controls
 *
 * Implements Auto/S/M shooting modes with runtime OV7670 register
 * adjustments for exposure, gain, brightness, contrast, and more.
 */
#ifndef SETTINGS_H
#define SETTINGS_H

/* Shooting modes */
#define SHOOT_AUTO   0   /* Full auto: AEC + AGC + night mode */
#define SHOOT_M      1   /* Manual: user controls shutter + gain, no night mode */

/* Shutter speed presets (frame rate presets, Manual mode only) */
#define SHUTTER_FAST    0   /* ~30 fps */
#define SHUTTER_NORMAL  1   /* ~16 fps */
#define SHUTTER_SLOW    2   /* ~8 fps */
#define SHUTTER_COUNT   3

/* ISO presets (gain ceiling for auto, direct gain for manual) */
#define ISO_100   0
#define ISO_200   1
#define ISO_400   2
#define ISO_800   3
#define ISO_1600  4
#define ISO_3200  5
#define ISO_COUNT 6

/* EV compensation steps (in 1/2 EV increments) */
#define EV_MIN  (-4)   /* -2.0 EV */
#define EV_MAX   4     /* +2.0 EV */

/* Night mode options */
#define NIGHT_OFF    0
#define NIGHT_HALF   1   /* 1/2 min frame rate */
#define NIGHT_QUARTER 2  /* 1/4 min frame rate */
#define NIGHT_EIGHTH 3   /* 1/8 min frame rate */

/* Camera settings state */
typedef struct {
    int shoot_mode;       /* SHOOT_AUTO / SHOOT_S / SHOOT_M */
    int shutter;          /* SHUTTER_FAST / NORMAL / SLOW */
    int iso;              /* ISO_100 .. ISO_3200 */
    int ev_comp;          /* EV_MIN .. EV_MAX (half-stops) */
    int brightness;       /* -128 to +127 */
    int contrast;         /* 0 to 127 */
    int night_mode;       /* NIGHT_OFF .. NIGHT_EIGHTH */
    int mirror;           /* 0 or 1 */
    int flip;             /* 0 or 1 */
    int show_hud;         /* 0 or 1 */
} camera_settings_t;

/* Global settings instance */
extern camera_settings_t cam_settings;

/* Initialize settings to defaults and apply to sensor */
void settings_init(void);

/* Apply the current shooting mode to the sensor (AEC/AGC enable/disable) */
void settings_apply_mode(void);

/* Apply shutter speed preset (changes CLKRC/DBLV) */
void settings_apply_shutter(void);

/* Apply ISO/gain settings */
void settings_apply_iso(void);

/* Apply EV compensation (AEW/AEB targets) */
void settings_apply_ev(void);

/* Apply brightness register */
void settings_apply_brightness(void);

/* Apply contrast register */
void settings_apply_contrast(void);

/* Apply night mode setting */
void settings_apply_night(void);

/* Apply mirror/flip */
void settings_apply_orientation(void);

/* Toggle shooting mode: Auto ↔ Manual */
void settings_cycle_mode(void);

/* Reset all settings to defaults and re-apply to sensor */
void settings_reset(void);

/* Adjust shutter speed: direction = +1 (faster) or -1 (slower) */
void settings_adjust_shutter(int direction);

/* Adjust ISO: direction = +1 (higher) or -1 (lower) */
void settings_adjust_iso(int direction);

/* Adjust EV compensation: direction = +1 (brighter) or -1 (darker) */
void settings_adjust_ev(int direction);

/* Adjust brightness: direction = +1 or -1 (steps of 16) */
void settings_adjust_brightness(int direction);

/* Adjust contrast: direction = +1 or -1 (steps of 8) */
void settings_adjust_contrast(int direction);

/* Toggle mirror */
void settings_toggle_mirror(void);

/* Toggle flip */
void settings_toggle_flip(void);

/* Toggle HUD display */
void settings_toggle_hud(void);

/* Get human-readable strings for HUD display */
const char *settings_mode_str(void);
const char *settings_shutter_str(void);
const char *settings_iso_str(void);

#endif /* SETTINGS_H */
