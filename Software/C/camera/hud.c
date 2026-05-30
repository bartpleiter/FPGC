/*
 * hud.c — Camera HUD overlay on GPU window layer
 *
 * Single-row status bar on the top row of the 40×25 tile window.
 * Layout (40 columns, left-to-right matches QA cycle order):
 *   Col 1-2:   Display mode (2b/3b/8b)
 *   Col 5-6:   Resolution (HI/LO)
 *   Col 9-12:  Shutter speed (1/30, 1/16, 1/8, 1/4)
 *   Col 15-18: Exposure (Full, 1/2, 1/4, 1/8, 1/16)
 *   Col 21-24: ISO (100..3200)
 *   Col 26-27: Brightness offset (+N/-N/ 0)
 *   Col 30-31: Contrast offset (+N/-N/ 0)
 *   Col 34-35: Gamma
 *   Col 38:    Exposure meter (custom bar pattern)
 *   Bottom row col 37-39: FPS
 *
 * The active quick-adjust parameter is shown with bracket markers
 * [xxx] when the highlight timer is active.
 */
#include "hud.h"
#include "settings.h"
#include "gpu_hal.h"
#include "gpu_data_ascii.h"
#include "fpgc.h"
#include "storage.h"
#include "viewfinder.h"

/* Quick-adjust parameter indices (must match viewfinder.c QA_* defines) */
#define QA_MODE        0
#define QA_RES         1
#define QA_SHUTTER     2
#define QA_EXPOSURE    3
#define QA_ISO         4
#define QA_BRIGHTNESS  5
#define QA_CONTRAST    6
#define QA_GAMMA       7

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

/* Custom bar-meter pattern indices (loaded in hud_init) */
#define METER_PAT_BASE  240   /* patterns 240-246: bar levels 0-6 */
#define METER_LEVELS    7
#define METER_PAL_IDX   29    /* custom palette: opaque near-black bg + white fg */

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

    /* Load custom bar-meter patterns at indices 240-246 */
    /* Uses custom palette (slot 29) with non-zero color0 so 2bpp=00 is OPAQUE:
     *   color0 [31:24] = 0x01 (near-black, opaque)
     *   color1 [23:16] = 0x01 (near-black)
     *   color2 [15:8]  = 0xFF (white)
     *   color3 [7:0]   = 0xFF (white)
     * Pattern encoding: 00=opaque black, 11=white
     * Frame: top/bottom all white (0xFFFF), sides left+right white (0xC003)
     */
    {
        unsigned int *pat;
        unsigned int *pal;
        pat = (unsigned int *)FPGC_GPU_PATTERN_TABLE;
        pal = (unsigned int *)FPGC_GPU_PALETTE_TABLE;

        /* Custom meter palette at slot 29 */
        pal[METER_PAL_IDX] = 0x0101FFFF;

        /* Level 0: frame only, interior opaque black */
        pat[METER_PAT_BASE * 4 + 0] = 0xFFFFC003;
        pat[METER_PAT_BASE * 4 + 1] = 0xC003C003;
        pat[METER_PAT_BASE * 4 + 2] = 0xC003C003;
        pat[METER_PAT_BASE * 4 + 3] = 0xC003FFFF;

        /* Level 1: bottom interior row filled */
        pat[(METER_PAT_BASE + 1) * 4 + 0] = 0xFFFFC003;
        pat[(METER_PAT_BASE + 1) * 4 + 1] = 0xC003C003;
        pat[(METER_PAT_BASE + 1) * 4 + 2] = 0xC003C003;
        pat[(METER_PAT_BASE + 1) * 4 + 3] = 0xFFFFFFFF;

        /* Level 2: bottom 2 interior rows filled */
        pat[(METER_PAT_BASE + 2) * 4 + 0] = 0xFFFFC003;
        pat[(METER_PAT_BASE + 2) * 4 + 1] = 0xC003C003;
        pat[(METER_PAT_BASE + 2) * 4 + 2] = 0xC003FFFF;
        pat[(METER_PAT_BASE + 2) * 4 + 3] = 0xFFFFFFFF;

        /* Level 3: bottom 3 interior rows filled */
        pat[(METER_PAT_BASE + 3) * 4 + 0] = 0xFFFFC003;
        pat[(METER_PAT_BASE + 3) * 4 + 1] = 0xC003C003;
        pat[(METER_PAT_BASE + 3) * 4 + 2] = 0xFFFFFFFF;
        pat[(METER_PAT_BASE + 3) * 4 + 3] = 0xFFFFFFFF;

        /* Level 4: bottom 4 interior rows filled */
        pat[(METER_PAT_BASE + 4) * 4 + 0] = 0xFFFFC003;
        pat[(METER_PAT_BASE + 4) * 4 + 1] = 0xC003FFFF;
        pat[(METER_PAT_BASE + 4) * 4 + 2] = 0xFFFFFFFF;
        pat[(METER_PAT_BASE + 4) * 4 + 3] = 0xFFFFFFFF;

        /* Level 5: bottom 5 interior rows filled */
        pat[(METER_PAT_BASE + 5) * 4 + 0] = 0xFFFFC003;
        pat[(METER_PAT_BASE + 5) * 4 + 1] = 0xFFFFFFFF;
        pat[(METER_PAT_BASE + 5) * 4 + 2] = 0xFFFFFFFF;
        pat[(METER_PAT_BASE + 5) * 4 + 3] = 0xFFFFFFFF;

        /* Level 6: all interior rows filled */
        pat[(METER_PAT_BASE + 6) * 4 + 0] = 0xFFFFFFFF;
        pat[(METER_PAT_BASE + 6) * 4 + 1] = 0xFFFFFFFF;
        pat[(METER_PAT_BASE + 6) * 4 + 2] = 0xFFFFFFFF;
        pat[(METER_PAT_BASE + 6) * 4 + 3] = 0xFFFFFFFF;
    }

    /* Clear entire window layer (transparent) */
    gpu_clear_window();
}

void hud_update(int fps, int qa_active, int qa_highlight, int avg_lum)
{
    int brt;
    int ct;
    const char *s;

    if (!cam_settings.show_hud) {
        hud_clear();
        return;
    }

    hud_clear_row(HUD_TOP_ROW);

    /* Display mode (cols 0–1) — QA-adjustable */
    {
        int pal;
        pal = (qa_highlight > 0 && qa_active == QA_MODE)
              ? HUD_PAL_HIGHLIGHT : HUD_PAL;
        if (qa_highlight > 0 && qa_active == QA_MODE)
            hud_puts(0, HUD_TOP_ROW, "[", pal);
        if (display_mode == MODE_DITH)
            hud_puts(1, HUD_TOP_ROW, "2b", pal);
        else if (display_mode == MODE_DITH8)
            hud_puts(1, HUD_TOP_ROW, "3b", pal);
        else
            hud_puts(1, HUD_TOP_ROW, "8b", pal);
        if (qa_highlight > 0 && qa_active == QA_MODE)
            hud_puts(3, HUD_TOP_ROW, "]", pal);
    }

    /* Resolution (cols 5–6) — QA-adjustable */
    {
        int pal;
        pal = (qa_highlight > 0 && qa_active == QA_RES)
              ? HUD_PAL_HIGHLIGHT : HUD_PAL;
        if (qa_highlight > 0 && qa_active == QA_RES)
            hud_puts(4, HUD_TOP_ROW, "[", pal);
        if (res_mode == RES_QQVGA)
            hud_puts(5, HUD_TOP_ROW, "LO", pal);
        else
            hud_puts(5, HUD_TOP_ROW, "HI", pal);
        if (qa_highlight > 0 && qa_active == QA_RES)
            hud_puts(7, HUD_TOP_ROW, "]", pal);
    }

    /* Shutter speed (cols 8–11) */
    {
        int pal;
        pal = (qa_highlight > 0 && qa_active == QA_SHUTTER)
              ? HUD_PAL_HIGHLIGHT : HUD_PAL;
        s = settings_shutter_str();
        if (qa_highlight > 0 && qa_active == QA_SHUTTER) {
            hud_puts(8, HUD_TOP_ROW, "[", pal);
            hud_puts(9, HUD_TOP_ROW, s, pal);
            hud_puts(13, HUD_TOP_ROW, "]", pal);
        } else {
            hud_puts(9, HUD_TOP_ROW, s, pal);
        }
    }

    /* Exposure (cols 14–18) */
    {
        int pal;
        pal = (qa_highlight > 0 && qa_active == QA_EXPOSURE)
              ? HUD_PAL_HIGHLIGHT : HUD_PAL;
        s = settings_exposure_str();
        if (qa_highlight > 0 && qa_active == QA_EXPOSURE) {
            hud_puts(14, HUD_TOP_ROW, "[", pal);
            hud_puts(15, HUD_TOP_ROW, s, pal);
            hud_puts(19, HUD_TOP_ROW, "]", pal);
        } else {
            hud_puts(15, HUD_TOP_ROW, s, pal);
        }
    }

    /* ISO (cols 20–24) */
    {
        int pal;
        pal = (qa_highlight > 0 && qa_active == QA_ISO)
              ? HUD_PAL_HIGHLIGHT : HUD_PAL;
        s = settings_iso_str();
        if (qa_highlight > 0 && qa_active == QA_ISO) {
            hud_puts(20, HUD_TOP_ROW, "[", pal);
            hud_puts(21, HUD_TOP_ROW, s, pal);
            hud_puts(25, HUD_TOP_ROW, "]", pal);
        } else {
            hud_puts(21, HUD_TOP_ROW, s, pal);
        }
    }

    /* Brightness offset (cols 25–27) */
    {
        int pal;
        pal = (qa_highlight > 0 && qa_active == QA_BRIGHTNESS)
              ? HUD_PAL_HIGHLIGHT : HUD_PAL;
        brt = cam_settings.brightness;
        if (qa_highlight > 0 && qa_active == QA_BRIGHTNESS)
            hud_puts(25, HUD_TOP_ROW, "[", pal);
        if (brt == 0)
            hud_puts(26, HUD_TOP_ROW, " 0", pal);
        else if (brt > 0) {
            hud_puts(26, HUD_TOP_ROW, "+", pal);
            hud_putint(27, HUD_TOP_ROW, brt, 1, pal);
        } else {
            hud_putint(26, HUD_TOP_ROW, brt, 2, pal);
        }
        if (qa_highlight > 0 && qa_active == QA_BRIGHTNESS)
            hud_puts(28, HUD_TOP_ROW, "]", pal);
    }

    /* Contrast offset (cols 29–31) */
    {
        int pal;
        pal = (qa_highlight > 0 && qa_active == QA_CONTRAST)
              ? HUD_PAL_HIGHLIGHT : HUD_PAL;
        ct = cam_settings.contrast;
        if (qa_highlight > 0 && qa_active == QA_CONTRAST)
            hud_puts(29, HUD_TOP_ROW, "[", pal);
        if (ct == 0)
            hud_puts(30, HUD_TOP_ROW, " 0", pal);
        else if (ct > 0) {
            hud_puts(30, HUD_TOP_ROW, "+", pal);
            hud_putint(31, HUD_TOP_ROW, ct, 1, pal);
        } else {
            hud_putint(30, HUD_TOP_ROW, ct, 2, pal);
        }
        if (qa_highlight > 0 && qa_active == QA_CONTRAST)
            hud_puts(32, HUD_TOP_ROW, "]", pal);
    }

    /* Gamma (cols 33–36) */
    {
        int pal;
        pal = (qa_highlight > 0 && qa_active == QA_GAMMA)
              ? HUD_PAL_HIGHLIGHT : HUD_PAL;
        s = settings_gamma_str();
        if (qa_highlight > 0 && qa_active == QA_GAMMA) {
            hud_puts(33, HUD_TOP_ROW, "[", pal);
            hud_puts(34, HUD_TOP_ROW, s, pal);
            hud_puts(37, HUD_TOP_ROW, "]", pal);
        } else {
            hud_puts(34, HUD_TOP_ROW, s, pal);
        }
    }

    /* Exposure meter (col 38): custom bar-graph tile */
    if (avg_lum >= 0) {
        int level;
        level = avg_lum / 43;
        if (level >= METER_LEVELS) level = METER_LEVELS - 1;
        gpu_write_window_tile(38, (unsigned int)HUD_TOP_ROW,
                              (unsigned int)(METER_PAT_BASE + level),
                              (unsigned int)METER_PAL_IDX);
    }
}

void hud_clear(void)
{
    hud_clear_row(HUD_TOP_ROW);
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
