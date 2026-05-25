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

/* ---- Layout constants ---- */
#define MENU_TOP_ROW     1   /* top border */
#define MENU_TITLE_ROW   2   /* "SETTINGS" */
#define MENU_FIRST_ITEM  4   /* first selectable item */
#define MENU_LAST_ITEM  17   /* last selectable item row */
#define MENU_HINT_ROW1  19
#define MENU_HINT_ROW2  20
#define MENU_BOT_ROW    21   /* bottom border */

#define MENU_ITEMS       14
#define MENU_LEFT         1   /* left border column */
#define MENU_RIGHT       38   /* right border column */
#define MENU_LABEL_COL    3   /* label start column */
#define MENU_VALUE_COL   25   /* value start column */

/* Palette */
#define MENU_PAL          PALETTE_WHITE_ON_BLACK
#define MENU_PAL_SEL      PALETTE_BLACK_ON_WHITE

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
extern int cam_settings_shoot_mode;  /* not used, we read cam_settings directly */

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
    menu_putchar(MENU_LEFT, y, left_ch, MENU_PAL);
    for (x = MENU_LEFT + 1; x < MENU_RIGHT; x++) {
        menu_putchar(x, y, BOX_HORIZ, MENU_PAL);
    }
    menu_putchar(MENU_RIGHT, y, right_ch, MENU_PAL);
}

/* Item labels (must match MENU_ITEMS = 14) */
static const char *item_labels[14] = {
    "Shutter",
    "Exposure",
    "ISO",
    "Brightness",
    "Contrast",
    "Sharpness",
    "Gamma",
    "Night mode",
    "Auto-contrast",
    "Display mode",
    "Resolution",
    "Mirror",
    "Flip",
    "Show HUD"
};

/* Which items are manual-only (greyed out in Auto mode)? */
static const int item_manual_only[14] = {
    1, 1, 1,   /* Shutter, Exposure, ISO */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/* Get the value string for item i */
static const char *item_value_str(int i)
{
    switch (i) {
    case 0:  return settings_shutter_str();
    case 1:  return settings_exposure_str();
    case 2:  return settings_iso_str();
    case 3:
        /* Brightness: format as +NNN or -NNN */
        {
            static char brt_buf[5];
            int v;
            int neg;
            int d;
            v = cam_settings.brightness;
            neg = 0;
            if (v < 0) { neg = 1; v = -v; }
            brt_buf[0] = neg ? '-' : '+';
            d = v / 100; brt_buf[1] = (char)('0' + d);
            v = v - d * 100;
            d = v / 10; brt_buf[2] = (char)('0' + d);
            v = v - d * 10;
            brt_buf[3] = (char)('0' + v);
            brt_buf[4] = 0;
            return brt_buf;
        }
    case 4:
        /* Contrast: format as NNN */
        {
            static char ctr_buf[4];
            int v;
            int d;
            v = cam_settings.contrast;
            d = v / 100; ctr_buf[0] = (char)('0' + d);
            v = v - d * 100;
            d = v / 10; ctr_buf[1] = (char)('0' + d);
            v = v - d * 10;
            ctr_buf[2] = (char)('0' + v);
            ctr_buf[3] = 0;
            return ctr_buf;
        }
    case 5:  return settings_sharpness_str();
    case 6:  return settings_gamma_str();
    case 7:  return settings_night_str();
    case 8:  return cam_settings.auto_contrast ? "On" : "Off";
    case 9:
        switch (display_mode) {
        case 0: return "RAW";
        case 1: return "DITH";
        case 2: return "DITH8";
        default: return "?";
        }
    case 10: return "QVGA";  /* resolution shown but changed via its own mechanism */
    case 11: return cam_settings.mirror ? "On" : "Off";
    case 12: return cam_settings.flip ? "On" : "Off";
    case 13: return cam_settings.show_hud ? "On" : "Off";
    default: return "?";
    }
}

/* Check if item is selectable in current mode */
static int item_selectable(int i)
{
    if (item_manual_only[i] && cam_settings.shoot_mode != SHOOT_M)
        return 0;
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
    case 0:  /* Shutter */
        settings_adjust_shutter(direction);
        return 3;
    case 1:  /* Exposure */
        settings_adjust_exposure(direction);
        return 3;
    case 2:  /* ISO */
        settings_adjust_iso(direction);
        return 3;
    case 3:  /* Brightness */
        settings_adjust_brightness(direction);
        return 5;
    case 4:  /* Contrast */
        settings_adjust_contrast(direction);
        return 5;
    case 5:  /* Sharpness */
        settings_adjust_sharpness(direction);
        return 5;
    case 6:  /* Gamma */
        settings_adjust_gamma(direction);
        return 5;
    case 7:  /* Night mode */
        cam_settings.night_mode = cam_settings.night_mode + direction;
        if (cam_settings.night_mode < 0) cam_settings.night_mode = 0;
        if (cam_settings.night_mode > 3) cam_settings.night_mode = 3;
        return 5;
    case 8:  /* Auto-contrast */
        settings_toggle_auto_contrast();
        return 0;  /* no I2C needed */
    case 9:  /* Display mode */
        {
            int dm;
            dm = display_mode + direction;
            if (dm < 0) dm = 0;
            if (dm > 2) dm = 2;
            display_mode = dm;
            return 0;
        }
    case 10: /* Resolution — returns special code */
        return 6;
    case 11: /* Mirror */
        settings_toggle_mirror();
        return 5;
    case 12: /* Flip */
        settings_toggle_flip();
        return 5;
    case 13: /* Show HUD */
        settings_toggle_hud();
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
    menu_putchar(MENU_LEFT, MENU_TITLE_ROW, BOX_VERT, MENU_PAL);
    menu_puts(MENU_LABEL_COL, MENU_TITLE_ROW, "SETTINGS", MENU_PAL);
    /* Fill rest of title row with spaces */
    {
        int x;
        for (x = MENU_LABEL_COL + 8; x < MENU_RIGHT; x++)
            menu_putchar(x, MENU_TITLE_ROW, ' ', MENU_PAL);
    }
    menu_putchar(MENU_RIGHT, MENU_TITLE_ROW, BOX_VERT, MENU_PAL);

    /* Blank row 3 */
    menu_putchar(MENU_LEFT, 3, BOX_VERT, MENU_PAL);
    {
        int x;
        for (x = MENU_LEFT + 1; x < MENU_RIGHT; x++)
            menu_putchar(x, 3, ' ', MENU_PAL);
    }
    menu_putchar(MENU_RIGHT, 3, BOX_VERT, MENU_PAL);

    /* Menu items (rows 4–17) */
    for (i = 0; i < MENU_ITEMS; i++) {
        int x;
        const char *label;
        const char *val;

        row = MENU_FIRST_ITEM + i;
        pal = MENU_PAL;
        if (i == menu_cursor) pal = MENU_PAL_SEL;

        /* Left border */
        menu_putchar(MENU_LEFT, row, BOX_VERT, MENU_PAL);

        /* Cursor indicator or space */
        if (i == menu_cursor)
            menu_putchar(MENU_LABEL_COL - 1, row, '>', pal);
        else
            menu_putchar(MENU_LABEL_COL - 1, row, ' ', MENU_PAL);

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
        menu_putchar(MENU_RIGHT, row, BOX_VERT, MENU_PAL);
    }

    /* Blank row 18 */
    menu_putchar(MENU_LEFT, 18, BOX_VERT, MENU_PAL);
    {
        int x;
        for (x = MENU_LEFT + 1; x < MENU_RIGHT; x++)
            menu_putchar(x, 18, ' ', MENU_PAL);
    }
    menu_putchar(MENU_RIGHT, 18, BOX_VERT, MENU_PAL);

    /* Hint rows */
    menu_putchar(MENU_LEFT, MENU_HINT_ROW1, BOX_VERT, MENU_PAL);
    menu_puts(MENU_LABEL_COL, MENU_HINT_ROW1, "[/] Navigate  [-/=] Adjust", MENU_PAL);
    {
        int x;
        for (x = MENU_LABEL_COL + 27; x < MENU_RIGHT; x++)
            menu_putchar(x, MENU_HINT_ROW1, ' ', MENU_PAL);
    }
    menu_putchar(MENU_RIGHT, MENU_HINT_ROW1, BOX_VERT, MENU_PAL);

    menu_putchar(MENU_LEFT, MENU_HINT_ROW2, BOX_VERT, MENU_PAL);
    menu_puts(MENU_LABEL_COL, MENU_HINT_ROW2, "[TAB] Close menu", MENU_PAL);
    {
        int x;
        for (x = MENU_LABEL_COL + 16; x < MENU_RIGHT; x++)
            menu_putchar(x, MENU_HINT_ROW2, ' ', MENU_PAL);
    }
    menu_putchar(MENU_RIGHT, MENU_HINT_ROW2, BOX_VERT, MENU_PAL);

    /* Bottom border */
    menu_draw_hline(MENU_BOT_ROW, BOX_BL, BOX_BR);
}

int menu_handle_key(int key)
{
    int action;

    if (key == 0) return 0;

    /* TAB closes menu */
    if (key == '\t') {
        menu_close();
        return 0;
    }

    /* Navigate up */
    if (key == '[' || key == ',') {
        cursor_move(-1);
        menu_draw();
        return 0;
    }

    /* Navigate down */
    if (key == ']' || key == '.') {
        cursor_move(1);
        menu_draw();
        return 0;
    }

    /* Adjust value */
    if (key == '-') {
        action = item_adjust(-1);
        menu_draw();
        return action;
    }
    if (key == '=') {
        action = item_adjust(1);
        menu_draw();
        return action;
    }

    return 0;
}
