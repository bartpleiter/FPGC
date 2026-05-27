/*
 * hud.c — Camera HUD overlay on GPU window layer
 *
 * Single-row status bar on the top row of the 40×25 tile window.
 * Layout (40 columns):
 *   Col 0-1:   Display mode (2b/3b/8b)
 *   Col 3-4:   Resolution (HI/LO)
 *   Col 7-10:  Shutter speed (1/30, 1/16, 1/8, 1/4)
 *   Col 13-16: ISO (100..3200)
 *   Col 18-19: Brightness offset (+N/-N/ 0)
 *   Col 22-23: Contrast offset (+N/-N/ 0)
 *   Col 37-39: FPS
 *
 * The active quick-adjust parameter is shown with bracket markers
 * [xxx] when the highlight timer is active.
 */
#include "hud.h"
#include "settings.h"
#include "gpu_hal.h"
#include "gpu_data_ascii.h"
#include "storage.h"
#include "viewfinder.h"

/* Quick-adjust parameter indices (must match viewfinder.c QA_* defines) */
#define QA_BRIGHTNESS  0
#define QA_CONTRAST    1
#define QA_SHUTTER     2
#define QA_EXPOSURE    3
#define QA_ISO         4
#define QA_GAMMA       5

/* External state from main.c and viewfinder.c */
extern int display_mode;
extern int res_mode;

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

void hud_update(int fps, int qa_active, int qa_highlight)
{
    int brt;
    int ct;
    const char *s;

    if (!cam_settings.show_hud) {
        hud_clear();
        return;
    }

    hud_clear_row(HUD_TOP_ROW);
    hud_clear_row(HUD_BOTTOM_ROW);

    /* Display mode (cols 0–1) */
    if (display_mode == MODE_DITH)
        hud_puts(0, HUD_TOP_ROW, "2b", HUD_PAL);
    else if (display_mode == MODE_DITH8)
        hud_puts(0, HUD_TOP_ROW, "3b", HUD_PAL);
    else
        hud_puts(0, HUD_TOP_ROW, "8b", HUD_PAL);

    /* Resolution (cols 3–4) */
    if (res_mode == RES_QQVGA)
        hud_puts(3, HUD_TOP_ROW, "LO", HUD_PAL);
    else
        hud_puts(3, HUD_TOP_ROW, "HI", HUD_PAL);

    /* Shutter speed (cols 7–10) */
    {
        int pal;
        pal = (qa_highlight > 0 && qa_active == QA_SHUTTER)
              ? HUD_PAL_HIGHLIGHT : HUD_PAL;
        s = settings_shutter_str();
        if (qa_highlight > 0 && qa_active == QA_SHUTTER) {
            hud_puts(6, HUD_TOP_ROW, "[", pal);
            hud_puts(7, HUD_TOP_ROW, s, pal);
            hud_puts(11, HUD_TOP_ROW, "]", pal);
        } else {
            hud_puts(7, HUD_TOP_ROW, s, pal);
        }
    }

    /* Exposure (col 12) — compact: show EV level */
    {
        int pal;
        pal = (qa_highlight > 0 && qa_active == QA_EXPOSURE)
              ? HUD_PAL_HIGHLIGHT : HUD_PAL;
        s = settings_exposure_str();
        if (qa_highlight > 0 && qa_active == QA_EXPOSURE) {
            hud_puts(12, HUD_TOP_ROW, "[", pal);
            hud_puts(13, HUD_TOP_ROW, s, pal);
        } else {
            hud_puts(13, HUD_TOP_ROW, s, pal);
        }
    }

    /* ISO (cols 16–19) */
    {
        int pal;
        pal = (qa_highlight > 0 && qa_active == QA_ISO)
              ? HUD_PAL_HIGHLIGHT : HUD_PAL;
        s = settings_iso_str();
        if (qa_highlight > 0 && qa_active == QA_ISO) {
            hud_puts(15, HUD_TOP_ROW, "[", pal);
            hud_puts(16, HUD_TOP_ROW, s, pal);
            hud_puts(20, HUD_TOP_ROW, "]", pal);
        } else {
            hud_puts(16, HUD_TOP_ROW, s, pal);
        }
    }

    /* Brightness offset (cols 22–24) */
    {
        int pal;
        pal = (qa_highlight > 0 && qa_active == QA_BRIGHTNESS)
              ? HUD_PAL_HIGHLIGHT : HUD_PAL;
        brt = cam_settings.brightness;
        if (qa_highlight > 0 && qa_active == QA_BRIGHTNESS)
            hud_puts(21, HUD_TOP_ROW, "[", pal);
        if (brt == 0)
            hud_puts(22, HUD_TOP_ROW, " 0", pal);
        else if (brt > 0) {
            hud_puts(22, HUD_TOP_ROW, "+", pal);
            hud_putint(23, HUD_TOP_ROW, brt, 1, pal);
        } else {
            hud_putint(22, HUD_TOP_ROW, brt, 2, pal);
        }
        if (qa_highlight > 0 && qa_active == QA_BRIGHTNESS)
            hud_puts(24, HUD_TOP_ROW, "]", pal);
    }

    /* Contrast offset (cols 26–28) */
    {
        int pal;
        pal = (qa_highlight > 0 && qa_active == QA_CONTRAST)
              ? HUD_PAL_HIGHLIGHT : HUD_PAL;
        ct = cam_settings.contrast;
        if (qa_highlight > 0 && qa_active == QA_CONTRAST)
            hud_puts(25, HUD_TOP_ROW, "[", pal);
        if (ct == 0)
            hud_puts(26, HUD_TOP_ROW, " 0", pal);
        else if (ct > 0) {
            hud_puts(26, HUD_TOP_ROW, "+", pal);
            hud_putint(27, HUD_TOP_ROW, ct, 1, pal);
        } else {
            hud_putint(26, HUD_TOP_ROW, ct, 2, pal);
        }
        if (qa_highlight > 0 && qa_active == QA_CONTRAST)
            hud_puts(28, HUD_TOP_ROW, "]", pal);
    }

    /* Gamma (cols 30–31) */
    {
        int pal;
        pal = (qa_highlight > 0 && qa_active == QA_GAMMA)
              ? HUD_PAL_HIGHLIGHT : HUD_PAL;
        s = settings_gamma_str();
        if (qa_highlight > 0 && qa_active == QA_GAMMA) {
            hud_puts(29, HUD_TOP_ROW, "[", pal);
            hud_puts(30, HUD_TOP_ROW, s, pal);
        } else {
            hud_puts(30, HUD_TOP_ROW, s, pal);
        }
    }

    /* SD indicator (col 33–34) */
    if (!storage_ready) {
        if (storage_sd_found)
            hud_puts(33, HUD_TOP_ROW, "!", HUD_PAL_WARN);
        else
            hud_puts(33, HUD_TOP_ROW, "-", HUD_PAL);
    }

    /* FPS (cols 37–39) */
    hud_putint(37, HUD_TOP_ROW, fps, 3, HUD_PAL);
}

void hud_clear(void)
{
    hud_clear_row(HUD_TOP_ROW);
    hud_clear_row(HUD_BOTTOM_ROW);
}

void hud_splash(const char *title)
{
    int len;
    int x;
    const char *p;

    gpu_clear_window();

    /* Center title on row 12 */
    len = 0;
    p = title;
    while (*p) { len = len + 1; p = p + 1; }
    x = (40 - len) / 2;

    hud_puts(x, 12, title, HUD_PAL);
}
