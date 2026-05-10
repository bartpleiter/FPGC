/*
 * hud.c — Camera HUD overlay on GPU window layer
 *
 * Uses the GPU's 40×25 tile window layer to overlay text on top
 * of the camera viewfinder (pixel layer). The window layer has
 * hardware transparency: tile index 0 with palette alpha=0 is
 * fully transparent, allowing the pixel layer to show through.
 *
 * Layout (40 columns × 30 rows at 8×8 pixels = 320×240):
 *   Row 0:  [MODE]              [SHUTTER]
 *   Row 1:                                     (empty)
 *   ...
 *   Row 28:                                    (empty)
 *   Row 29: ISO:xxxx  ±x.xEV         xxfps
 *
 * Note: The window layer is 40×25 tiles covering 320×200 pixels.
 * The bottom 40 pixels (rows 25-29 of the display) are NOT covered
 * by the window layer. We use rows 0 and 24 for top/bottom HUD.
 */
#include "hud.h"
#include "settings.h"
#include "gpu_hal.h"
#include "gpu_data_ascii.h"

/* HUD row positions */
#define HUD_TOP_ROW     0
#define HUD_BOTTOM_ROW  24

/* Palette for HUD text */
#define HUD_PAL  PALETTE_WHITE_ON_BLACK
#define HUD_PAL_HIGHLIGHT PALETTE_GREEN_ON_BLACK
#define HUD_PAL_WARN PALETTE_RED_ON_BLACK

/* Write a string to the window layer at (x, y) with given palette */
static void hud_puts(int x, int y, const char *s, int pal)
{
    while (*s) {
        if (x >= 40) break;
        gpu_write_window_tile((unsigned int)x, (unsigned int)y,
                              (unsigned int)(unsigned char)*s,
                              (unsigned int)pal);
        x = x + 1;
        s = s + 1;
    }
}

/* Write an integer (right-aligned in field_width) to the window layer */
static void hud_putint(int x, int y, int val, int field_width, int pal)
{
    char buf[12];
    int i;
    int neg;
    int pos;

    neg = 0;
    if (val < 0) {
        neg = 1;
        val = -val;
    }

    /* Convert to string (reversed) */
    i = 0;
    if (val == 0) {
        buf[i] = '0';
        i = i + 1;
    } else {
        while (val > 0) {
            buf[i] = (char)('0' + (val % 10));
            val = val / 10;
            i = i + 1;
        }
    }
    if (neg) {
        buf[i] = '-';
        i = i + 1;
    }

    /* Pad with spaces */
    pos = x + field_width - i;
    while (pos > x) {
        pos = pos - 1;
        gpu_write_window_tile((unsigned int)pos, (unsigned int)y,
                              (unsigned int)' ', (unsigned int)pal);
    }

    /* Write digits (reverse order) */
    pos = x + field_width - i;
    while (i > 0) {
        i = i - 1;
        gpu_write_window_tile((unsigned int)pos, (unsigned int)y,
                              (unsigned int)(unsigned char)buf[i],
                              (unsigned int)pal);
        pos = pos + 1;
    }
}

/* Clear a row of the window layer (write transparent tiles) */
static void hud_clear_row(int y)
{
    int x;
    for (x = 0; x < 40; x++) {
        gpu_write_window_tile((unsigned int)x, (unsigned int)y, 0, 0);
    }
}

void hud_init(void)
{
    /* Load ASCII font patterns and color palettes */
    gpu_load_pattern_table(gpu_default_patterns);
    gpu_load_palette_table(gpu_default_palette);

    /* Clear entire window layer (transparent) */
    gpu_clear_window();
}

void hud_update(int fps)
{
    int ev;
    char ev_buf[8];
    int ev_idx;

    if (!cam_settings.show_hud) {
        hud_clear();
        return;
    }

    /* ---- Top row: mode + shutter speed ---- */
    hud_clear_row(HUD_TOP_ROW);

    /* Mode indicator (left side) */
    hud_puts(0, HUD_TOP_ROW, "[", HUD_PAL);
    hud_puts(1, HUD_TOP_ROW, settings_mode_str(), HUD_PAL_HIGHLIGHT);
    hud_puts(2, HUD_TOP_ROW, "]", HUD_PAL);

    /* Shutter speed (right side, Manual mode only) */
    if (cam_settings.shoot_mode == SHOOT_M) {
        hud_puts(34, HUD_TOP_ROW, settings_shutter_str(), HUD_PAL);
    }

    /* ---- Bottom row: ISO, EV, FPS ---- */
    hud_clear_row(HUD_BOTTOM_ROW);

    /* ISO (left, Manual mode only) */
    if (cam_settings.shoot_mode == SHOOT_M) {
        hud_puts(0, HUD_BOTTOM_ROW, "ISO:", HUD_PAL);
        hud_puts(4, HUD_BOTTOM_ROW, settings_iso_str(), HUD_PAL);
    }

    /* EV compensation (center, only in Auto/S) */
    if (cam_settings.shoot_mode != SHOOT_M) {
        ev = cam_settings.ev_comp;  /* in half-stops */
        ev_idx = 0;

        if (ev >= 0) {
            ev_buf[ev_idx] = '+';
        } else {
            ev_buf[ev_idx] = '-';
            ev = -ev;
        }
        ev_idx = ev_idx + 1;

        /* Integer part */
        ev_buf[ev_idx] = (char)('0' + (ev / 2));
        ev_idx = ev_idx + 1;
        ev_buf[ev_idx] = '.';
        ev_idx = ev_idx + 1;
        /* Fractional part (0 or 5) */
        ev_buf[ev_idx] = (ev & 1) ? '5' : '0';
        ev_idx = ev_idx + 1;
        ev_buf[ev_idx] = 'E';
        ev_idx = ev_idx + 1;
        ev_buf[ev_idx] = 'V';
        ev_idx = ev_idx + 1;
        ev_buf[ev_idx] = '\0';

        hud_puts(14, HUD_BOTTOM_ROW, ev_buf, HUD_PAL);
    }

    /* FPS (right) */
    hud_putint(35, HUD_BOTTOM_ROW, fps, 3, HUD_PAL);
    hud_puts(38, HUD_BOTTOM_ROW, "fp", HUD_PAL);
}

void hud_clear(void)
{
    hud_clear_row(HUD_TOP_ROW);
    hud_clear_row(HUD_BOTTOM_ROW);
}
