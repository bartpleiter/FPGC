/*
 * menu.c — On-screen settings menu
 *
 * Renders a 14-item settings menu on the GPU window tile layer
 * using box-drawing characters (CP437 chars 179, 191, 192, 196,
 * 217, 218). The live viewfinder continues running behind the menu.
 */
#include "menu.h"
#include "settings.h"
#include "gpu_hal.h"
#include "gpu_data_ascii.h"
#include "viewfinder.h"

#include "storage.h"

/* ---- Layout constants ---- */
#define MENU_TOP_ROW     1   /* top border */
#define MENU_TITLE_ROW   2   /* "SETTINGS" */
#define MENU_FIRST_ITEM  4   /* first selectable item */
#define MENU_LAST_ITEM  19   /* last selectable item row (FIRST_ITEM + ITEMS - 1) */
#define MENU_HINT_ROW1  22
#define MENU_HINT_ROW2  23
#define MENU_BOT_ROW    24   /* bottom border */

#define MENU_ITEMS       16
#define MENU_LEFT         1   /* left border column */
#define MENU_RIGHT       38   /* right border column */
#define MENU_LABEL_COL    3   /* label start column */
#define MENU_VALUE_COL   25   /* value start column */

/* Palette */
#define MENU_PAL_IDX      PALETTE_WHITE_ON_BLACK
#define MENU_PAL_SEL_IDX  PALETTE_BLACK_ON_WHITE

/* CP437 box-drawing characters */
#define BOX_HORIZ   196
#define BOX_VERT    179
#define BOX_TL      218
#define BOX_TR      191
#define BOX_BL      192
#define BOX_BR      217

/* ---- State ---- */
static int menu_visible;
static int menu_cursor;

/* ---- External state from viewfinder/main ---- */
extern int display_mode;
extern void set_mode(int mode);
extern int res_mode;

/* ---- Private helpers ---- */

static void menu_puts(int x, int y, const char *s, int pal)
{
    while (*s) {
        if (x > MENU_RIGHT) break;
        gpu_write_window_tile((unsigned int)x, (unsigned int)y,
                              (unsigned int)(unsigned char)*s,
                              (unsigned int)pal);
        x = x + 1;
        s = s + 1;
    }
}

static void menu_putchar(int x, int y, int ch, int pal)
{
    gpu_write_window_tile((unsigned int)x, (unsigned int)y,
                          (unsigned int)ch, (unsigned int)pal);
}

/* Clear rows used by the menu (write transparent tiles) */
static void menu_clear(void)
{
    int y;
    int x;
    for (y = MENU_TOP_ROW; y <= MENU_BOT_ROW; y++) {
        for (x = 0; x < 40; x++) {
            gpu_write_window_tile((unsigned int)x, (unsigned int)y, 0, 0);
        }
    }
}

/* Draw horizontal border line */
static void menu_draw_hline(int y, int left_ch, int right_ch)
{
    int x;
    menu_putchar(MENU_LEFT, y, left_ch, MENU_PAL_IDX);
    for (x = MENU_LEFT + 1; x < MENU_RIGHT; x++) {
        menu_putchar(x, y, BOX_HORIZ, MENU_PAL_IDX);
    }
    menu_putchar(MENU_RIGHT, y, right_ch, MENU_PAL_IDX);
}

/* Item labels (must match MENU_ITEMS = 16) */
static const char *item_labels[16] = {
    "Gallery",
    "Load preset",
    "Save preset",
    "Shutter",
    "Exposure",
    "ISO",
    "Brightness",
    "Contrast",
    "Sharpness",
    "Gamma",
    "Display mode",
    "Resolution",
    "Mirror",
    "Flip",
    "Show HUD",
    "Storage"
};

/* Preset slot selection state */
static int preset_slot = 0;  /* 0..2 */

/* Get the value string for item i */
static const char *item_value_str(int i)
{
    switch (i) {
    case 0:  return ">>>";  /* Gallery */
    case 1:
    case 2:
        /* Load/Save preset: show slot number */
        {
            static char slot_buf[2];
            slot_buf[0] = (char)('1' + preset_slot);
            slot_buf[1] = 0;
            return slot_buf;
        }
    case 3:  return settings_shutter_str();
    case 4:  return settings_exposure_str();
    case 5:  return settings_iso_str();
    case 6:
        /* Brightness offset: show as +3, -2, 0 */
        {
            static char brt_buf[4];
            int v;
            v = cam_settings.brightness;
            if (v == 0) {
                brt_buf[0] = ' '; brt_buf[1] = '0'; brt_buf[2] = 0;
            } else if (v > 0) {
                brt_buf[0] = '+';
                brt_buf[1] = (char)('0' + v);
                brt_buf[2] = 0;
            } else {
                brt_buf[0] = '-';
                brt_buf[1] = (char)('0' + (-v));
                brt_buf[2] = 0;
            }
            return brt_buf;
        }
    case 7:
        /* Contrast offset: show as +4, -1, 0 */
        {
            static char ctr_buf[4];
            int v;
            v = cam_settings.contrast;
            if (v == 0) {
                ctr_buf[0] = ' '; ctr_buf[1] = '0'; ctr_buf[2] = 0;
            } else if (v > 0) {
                ctr_buf[0] = '+';
                ctr_buf[1] = (char)('0' + v);
                ctr_buf[2] = 0;
            } else {
                ctr_buf[0] = '-';
                ctr_buf[1] = (char)('0' + (-v));
                ctr_buf[2] = 0;
            }
            return ctr_buf;
        }
    case 8:  return settings_sharpness_str();
    case 9:  return settings_gamma_str();
    case 10:
        switch (display_mode) {
        case 0: return "Full8b";
        case 1: return "Dith2b";
        case 2: return "Dith3b";
        default: return "?";
        }
    case 11:
        if (res_mode == RES_QQVGA) return "160x120";
        return "320x240";
    case 12: return cam_settings.mirror ? "On" : "Off";
    case 13: return cam_settings.flip ? "On" : "Off";
    case 14: return cam_settings.show_hud ? "On" : "Off";
    case 15:
        /* Storage: remaining images */
        {
            static char sto_buf[12];
            int rem;
            int pos;
            int d;
            if (!storage_ready) return "No SD";
            rem = storage_remaining_images(res_mode);
            pos = 0;
            if (rem >= 10000) { d = rem / 10000; sto_buf[pos] = (char)('0' + d); pos = pos + 1; rem = rem - d * 10000; }
            if (rem >= 1000 || pos > 0) { d = rem / 1000; sto_buf[pos] = (char)('0' + d); pos = pos + 1; rem = rem - d * 1000; }
            if (rem >= 100 || pos > 0) { d = rem / 100; sto_buf[pos] = (char)('0' + d); pos = pos + 1; rem = rem - d * 100; }
            if (rem >= 10 || pos > 0) { d = rem / 10; sto_buf[pos] = (char)('0' + d); pos = pos + 1; rem = rem - d * 10; }
            sto_buf[pos] = (char)('0' + rem); pos = pos + 1;
            sto_buf[pos] = ' '; pos = pos + 1;
            sto_buf[pos] = 'f'; pos = pos + 1;
            sto_buf[pos] = 'r'; pos = pos + 1;
            sto_buf[pos] = 'e'; pos = pos + 1;
            sto_buf[pos] = 'e'; pos = pos + 1;
            sto_buf[pos] = 0;
            return sto_buf;
        }
    default: return "?";
    }
}

/* Check if item is selectable */
static int item_selectable(int i)
{
    /* All items are always selectable (manual-only mode) */
    (void)i;
    return 1;
}

/* Move cursor to next selectable item in given direction */
static void cursor_move(int direction)
{
    int i;
    int next;
    next = menu_cursor;
    for (i = 0; i < MENU_ITEMS; i++) {
        next = next + direction;
        if (next < 0) next = MENU_ITEMS - 1;
        if (next >= MENU_ITEMS) next = 0;
        if (item_selectable(next)) {
            menu_cursor = next;
            return;
        }
    }
}

/* Adjust the value for the currently selected item.
 * Returns the pending_action code needed. */
static int item_adjust(int direction)
{
    switch (menu_cursor) {
    case 0:  /* Gallery */
        return 7;  /* signal gallery entry */
    case 1: /* Load preset — J/L cycles slot */
        preset_slot = preset_slot + direction;
        if (preset_slot < 0) preset_slot = PRESET_COUNT - 1;
        if (preset_slot >= PRESET_COUNT) preset_slot = 0;
        return 0;
    case 2: /* Save preset — J/L cycles slot */
        preset_slot = preset_slot + direction;
        if (preset_slot < 0) preset_slot = PRESET_COUNT - 1;
        if (preset_slot >= PRESET_COUNT) preset_slot = 0;
        return 0;
    case 3:  /* Shutter */
        settings_adjust_shutter(direction);
        return 3;
    case 4:  /* Exposure */
        settings_adjust_exposure(direction);
        return 3;
    case 5:  /* ISO */
        settings_adjust_iso(direction);
        return 3;
    case 6:  /* Brightness */
        settings_adjust_brightness(direction);
        return 5;
    case 7:  /* Contrast */
        settings_adjust_contrast(direction);
        return 5;
    case 8:  /* Sharpness */
        settings_adjust_sharpness(direction);
        return 5;
    case 9:  /* Gamma */
        settings_adjust_gamma(direction);
        return 5;
    case 10:  /* Display mode */
        {
            int dm;
            dm = display_mode + direction;
            if (dm < 0) dm = 0;
            if (dm > 2) dm = 2;
            set_mode(dm);
            return 0;
        }
    case 11: /* Resolution — returns special code */
        return 6;
    case 12: /* Mirror */
        settings_toggle_mirror();
        return 5;
    case 13: /* Flip */
        settings_toggle_flip();
        return 5;
    case 14: /* Show HUD */
        settings_toggle_hud();
        return 0;
    case 15: /* Storage — read-only */
        return 0;
    default:
        return 0;
    }
}

/* ---- Public API ---- */

void menu_open(void)
{
    menu_visible = 1;
    menu_cursor = 0;
    /* Ensure cursor lands on a selectable item */
    if (!item_selectable(menu_cursor))
        cursor_move(1);
    menu_draw();
}

void menu_close(void)
{
    menu_visible = 0;
    menu_clear();
}

int menu_is_open(void)
{
    return menu_visible;
}

void menu_draw(void)
{
    int i;
    int row;
    int pal;

    /* Top border */
    menu_draw_hline(MENU_TOP_ROW, BOX_TL, BOX_TR);

    /* Title row */
    menu_putchar(MENU_LEFT, MENU_TITLE_ROW, BOX_VERT, MENU_PAL_IDX);
    menu_puts(MENU_LABEL_COL, MENU_TITLE_ROW, "SETTINGS", MENU_PAL_IDX);
    /* Fill rest of title row with spaces */
    {
        int x;
        for (x = MENU_LABEL_COL + 8; x < MENU_RIGHT; x++)
            menu_putchar(x, MENU_TITLE_ROW, ' ', MENU_PAL_IDX);
    }
    menu_putchar(MENU_RIGHT, MENU_TITLE_ROW, BOX_VERT, MENU_PAL_IDX);

    /* Blank row 3 */
    menu_putchar(MENU_LEFT, 3, BOX_VERT, MENU_PAL_IDX);
    {
        int x;
        for (x = MENU_LEFT + 1; x < MENU_RIGHT; x++)
            menu_putchar(x, 3, ' ', MENU_PAL_IDX);
    }
    menu_putchar(MENU_RIGHT, 3, BOX_VERT, MENU_PAL_IDX);

    /* Menu items (rows 4–17) */
    for (i = 0; i < MENU_ITEMS; i++) {
        int x;
        const char *label;
        const char *val;

        row = MENU_FIRST_ITEM + i;
        pal = MENU_PAL_IDX;
        if (i == menu_cursor) pal = MENU_PAL_SEL_IDX;

        /* Left border */
        menu_putchar(MENU_LEFT, row, BOX_VERT, MENU_PAL_IDX);

        /* Cursor indicator or space */
        if (i == menu_cursor)
            menu_putchar(MENU_LABEL_COL - 1, row, '>', pal);
        else
            menu_putchar(MENU_LABEL_COL - 1, row, ' ', MENU_PAL_IDX);

        /* Label */
        label = item_labels[i];
        x = MENU_LABEL_COL;
        while (*label && x < MENU_VALUE_COL) {
            menu_putchar(x, row, (unsigned char)*label, pal);
            x = x + 1;
            label = label + 1;
        }
        /* Pad label to value column */
        while (x < MENU_VALUE_COL) {
            menu_putchar(x, row, ' ', pal);
            x = x + 1;
        }

        /* Value */
        val = item_value_str(i);
        while (*val && x < MENU_RIGHT) {
            menu_putchar(x, row, (unsigned char)*val, pal);
            x = x + 1;
            val = val + 1;
        }
        /* Pad to right border */
        while (x < MENU_RIGHT) {
            menu_putchar(x, row, ' ', pal);
            x = x + 1;
        }

        /* Right border */
        menu_putchar(MENU_RIGHT, row, BOX_VERT, MENU_PAL_IDX);
    }

    /* Blank row after items */
    menu_putchar(MENU_LEFT, MENU_LAST_ITEM + 1, BOX_VERT, MENU_PAL_IDX);
    {
        int x;
        for (x = MENU_LEFT + 1; x < MENU_RIGHT; x++)
            menu_putchar(x, MENU_LAST_ITEM + 1, ' ', MENU_PAL_IDX);
    }
    menu_putchar(MENU_RIGHT, MENU_LAST_ITEM + 1, BOX_VERT, MENU_PAL_IDX);

    /* Second blank row */
    menu_putchar(MENU_LEFT, MENU_LAST_ITEM + 2, BOX_VERT, MENU_PAL_IDX);
    {
        int x;
        for (x = MENU_LEFT + 1; x < MENU_RIGHT; x++)
            menu_putchar(x, MENU_LAST_ITEM + 2, ' ', MENU_PAL_IDX);
    }
    menu_putchar(MENU_RIGHT, MENU_LAST_ITEM + 2, BOX_VERT, MENU_PAL_IDX);

    /* Hint rows */
    menu_putchar(MENU_LEFT, MENU_HINT_ROW1, BOX_VERT, MENU_PAL_IDX);
    menu_puts(MENU_LABEL_COL, MENU_HINT_ROW1, "I/K Nav  J/L Adj  Spc Open", MENU_PAL_IDX);
    {
        int x;
        for (x = MENU_LABEL_COL + 27; x < MENU_RIGHT; x++)
            menu_putchar(x, MENU_HINT_ROW1, ' ', MENU_PAL_IDX);
    }
    menu_putchar(MENU_RIGHT, MENU_HINT_ROW1, BOX_VERT, MENU_PAL_IDX);

    menu_putchar(MENU_LEFT, MENU_HINT_ROW2, BOX_VERT, MENU_PAL_IDX);
    menu_puts(MENU_LABEL_COL, MENU_HINT_ROW2, "M Close menu", MENU_PAL_IDX);
    {
        int x;
        for (x = MENU_LABEL_COL + 12; x < MENU_RIGHT; x++)
            menu_putchar(x, MENU_HINT_ROW2, ' ', MENU_PAL_IDX);
    }
    menu_putchar(MENU_RIGHT, MENU_HINT_ROW2, BOX_VERT, MENU_PAL_IDX);

    /* Bottom border */
    menu_draw_hline(MENU_BOT_ROW, BOX_BL, BOX_BR);
}

int menu_handle_key(int key)
{
    int action;

    if (key == 0) return 0;

    /* Navigate up (I) */
    if (key == 'i' || key == 'I') {
        cursor_move(-1);
        menu_draw();
        return 0;
    }

    /* Navigate down (K) */
    if (key == 'k' || key == 'K') {
        cursor_move(1);
        menu_draw();
        return 0;
    }

    /* Adjust value left (J) */
    if (key == 'j' || key == 'J') {
        action = item_adjust(-1);
        menu_draw();
        return action;
    }

    /* Adjust value right (L) */
    if (key == 'l' || key == 'L') {
        action = item_adjust(1);
        menu_draw();
        return action;
    }

    /* Confirm (Space) — for preset load/save and gallery */
    if (key == ' ') {
        if (menu_cursor == 0) {
            /* Gallery */
            return 7;
        }
        if (menu_cursor == 1) {
            /* Load preset — always restart viewfinder (res/mode may change) */
            if (settings_load_preset(preset_slot) == 0) {
                return 1;  /* resolution switch path — full reinit */
            }
            return 0;
        }
        if (menu_cursor == 2) {
            /* Save preset */
            settings_save_preset(preset_slot);
            menu_draw();
            return 0;
        }
        return 0;
    }

    return 0;
}
