/*
 * libterm v2 — see term2.h for design.
 *
 * Memory layout per screen:
 *   tiles[h][w]    — character code at each cell
 *   palettes[h][w] — palette index at each cell
 * Two screens are kept (primary + alt). Scrollback history is only
 * maintained for the primary screen.
 *
 * The library does no I/O directly; every cell change is reported via
 * render_cell_fn so the same code is used by BDOS (push to GPU window
 * plane) and by host tests (record into an array).
 */

#include "term2.h"
#include <stddef.h>

/* When CPP=cproc on BDOS, no <string.h> overhead — we provide our own */
#ifdef TERM2_HOST_TEST
#include <string.h>
#else
static void *term2_memset(void *d, int c, unsigned int n) {
    unsigned char *p = (unsigned char *)d;
    while (n--) *p++ = (unsigned char)c;
    return d;
}
static void *term2_memcpy(void *d, const void *s, unsigned int n) {
    unsigned char *dp = (unsigned char *)d;
    const unsigned char *sp = (const unsigned char *)s;
    while (n--) *dp++ = *sp++;
    return d;
}
#define memset term2_memset
#define memcpy term2_memcpy
#endif

/* ============================================================ State ===== */

typedef struct {
    unsigned char tiles[TERM2_MAX_HEIGHT][TERM2_MAX_WIDTH];
    unsigned char palettes[TERM2_MAX_HEIGHT][TERM2_MAX_WIDTH];
    int cursor_x;
    int cursor_y;
    int saved_cx;        /* DECSC / ESC[s */
    int saved_cy;
    unsigned char palette;
    unsigned char saved_palette;
    int scroll_top;      /* 0-based inclusive */
    int scroll_bot;      /* 0-based inclusive */
    int cursor_visible;
} term2_screen_t;

static int g_width  = 40;
static int g_height = 25;
static term2_render_cell_fn g_render = NULL;
static term2_uart_mirror_fn g_uart   = NULL;

/* Two screens: primary [0], alt [1] */
static term2_screen_t g_screens[2];
static int g_active = 0;        /* index into g_screens */

/* Scrollback (primary only) */
static unsigned char g_hist_tiles[TERM2_HISTORY_LINES][TERM2_MAX_WIDTH];
static unsigned char g_hist_pal[TERM2_HISTORY_LINES][TERM2_MAX_WIDTH];
static int g_hist_head = 0;
static int g_hist_count = 0;
static int g_scroll_view_offset = 0;   /* 0 = looking at live; >0 = N rows back */

/* ANSI parser state */
typedef enum {
    PS_NORMAL,
    PS_ESC,
    PS_CSI
} parser_state_t;

#define CSI_MAX_PARAMS 8
static parser_state_t g_pstate = PS_NORMAL;
static int g_csi_params[CSI_MAX_PARAMS];
static int g_csi_nparams = 0;
static int g_csi_have_param = 0;
static int g_csi_question = 0;     /* '?' prefix (private CSI) */

#define ACTIVE() (&g_screens[g_active])

/* ====================================================== Forward decls ===== */
static void render(int x, int y, unsigned char t, unsigned char p);
static void put_at(int x, int y, unsigned char t, unsigned char p);
static void emit_printable(unsigned char c);
static void newline(void);
static void scroll_up_in_region(int n);
static void scroll_down_in_region(int n);
static void parser_step(unsigned char c);
static void parser_dispatch_csi(unsigned char final);
static void clear_screen_full(void);

/* ============================================================ Helpers ===== */

static void render(int x, int y, unsigned char t, unsigned char p) {
    if (g_render && x >= 0 && x < g_width && y >= 0 && y < g_height)
        g_render(x, y, t, p);
}

static void put_at(int x, int y, unsigned char t, unsigned char p) {
    if (x < 0 || x >= g_width || y < 0 || y >= g_height) return;
    term2_screen_t *s = ACTIVE();
    s->tiles[y][x] = t;
    s->palettes[y][x] = p;
    /* Don't render if we're scrolled back in primary; the user's view is
       showing history, not live. The next snap_to_bottom will refresh. */
    if (g_active == 0 && g_scroll_view_offset > 0) return;
    render(x, y, t, p);
}

static void clear_region(int top, int bot, unsigned char palette_for_clear) {
    int y, x;
    term2_screen_t *s = ACTIVE();
    for (y = top; y <= bot; y++) {
        for (x = 0; x < g_width; x++) {
            s->tiles[y][x] = 0;
            s->palettes[y][x] = palette_for_clear;
            if (!(g_active == 0 && g_scroll_view_offset > 0))
                render(x, y, 0, palette_for_clear);
        }
    }
}

static void clear_screen_full(void) {
    term2_screen_t *s = ACTIVE();
    clear_region(0, g_height - 1, s->palette);
    s->cursor_x = 0;
    s->cursor_y = 0;
}

/* ====================================================== Scroll & history === */

static void push_to_history(int row_idx) {
    /* Only the primary screen contributes to scrollback history. */
    if (g_active != 0) return;
    term2_screen_t *s = ACTIVE();
    int x;
    for (x = 0; x < g_width; x++) {
        g_hist_tiles[g_hist_head][x] = s->tiles[row_idx][x];
        g_hist_pal[g_hist_head][x]   = s->palettes[row_idx][x];
    }
    g_hist_head = (g_hist_head + 1) % TERM2_HISTORY_LINES;
    if (g_hist_count < TERM2_HISTORY_LINES) g_hist_count++;
    if (g_scroll_view_offset > 0) g_scroll_view_offset = 0;
}

static void scroll_up_in_region(int n) {
    /* "Scroll up" = content moves up; new blank rows appear at the bottom.
       This is the standard behavior at end-of-screen. */
    term2_screen_t *s = ACTIVE();
    int top = s->scroll_top;
    int bot = s->scroll_bot;
    int region_h = bot - top + 1;
    int i, x, y;
    if (n <= 0) return;
    if (n > region_h) n = region_h;

    /* Push displaced rows to history (only when scrolling the full screen). */
    if (top == 0 && bot == g_height - 1) {
        for (i = 0; i < n; i++)
            push_to_history(i);
    }

    /* Shift rows up by n */
    for (y = top; y + n <= bot; y++) {
        for (x = 0; x < g_width; x++) {
            s->tiles[y][x]    = s->tiles[y + n][x];
            s->palettes[y][x] = s->palettes[y + n][x];
            if (!(g_active == 0 && g_scroll_view_offset > 0))
                render(x, y, s->tiles[y][x], s->palettes[y][x]);
        }
    }
    /* Clear the n bottom rows of the region */
    for (y = bot - n + 1; y <= bot; y++) {
        for (x = 0; x < g_width; x++) {
            s->tiles[y][x]    = 0;
            s->palettes[y][x] = s->palette;
            if (!(g_active == 0 && g_scroll_view_offset > 0))
                render(x, y, 0, s->palette);
        }
    }
}

static void scroll_down_in_region(int n) {
    term2_screen_t *s = ACTIVE();
    int top = s->scroll_top;
    int bot = s->scroll_bot;
    int region_h = bot - top + 1;
    int x, y;
    if (n <= 0) return;
    if (n > region_h) n = region_h;

    /* No history push (we're inserting blank rows, not displacing). */
    for (y = bot; y - n >= top; y--) {
        for (x = 0; x < g_width; x++) {
            s->tiles[y][x]    = s->tiles[y - n][x];
            s->palettes[y][x] = s->palettes[y - n][x];
            if (!(g_active == 0 && g_scroll_view_offset > 0))
                render(x, y, s->tiles[y][x], s->palettes[y][x]);
        }
    }
    for (y = top; y < top + n; y++) {
        for (x = 0; x < g_width; x++) {
            s->tiles[y][x]    = 0;
            s->palettes[y][x] = s->palette;
            if (!(g_active == 0 && g_scroll_view_offset > 0))
                render(x, y, 0, s->palette);
        }
    }
}

/* ============================================================ Cursor I/O === */

static void newline(void) {
    term2_screen_t *s = ACTIVE();
    s->cursor_x = 0;
    s->cursor_y++;
    if (s->cursor_y > s->scroll_bot) {
        scroll_up_in_region(1);
        s->cursor_y = s->scroll_bot;
    }
    if (s->cursor_y >= g_height) s->cursor_y = g_height - 1;
}

static void emit_printable(unsigned char c) {
    term2_screen_t *s = ACTIVE();
    /* Auto-snap to bottom on output if the user was viewing scrollback. */
    if (g_active == 0 && g_scroll_view_offset > 0)
        term2_snap_to_bottom();

    if (g_uart) g_uart((char)c);

    put_at(s->cursor_x, s->cursor_y, c, s->palette);
    s->cursor_x++;
    if (s->cursor_x >= g_width) {
        newline();
    }
}

/* ============================================================ ANSI parser === */

static int csi_param_or(int idx, int defval) {
    if (idx >= g_csi_nparams) return defval;
    if (g_csi_params[idx] == 0 && !(idx == 0 && g_csi_have_param)) return defval;
    return g_csi_params[idx];
}

/* For CSI Pn commands where 0 explicitly means 0 (e.g. SGR), this returns the
   actual stored value with a default of 0 if no param was given. */
static int csi_param(int idx, int defval) {
    if (idx >= g_csi_nparams) return defval;
    return g_csi_params[idx];
}

static void parser_reset_csi(void) {
    int i;
    for (i = 0; i < CSI_MAX_PARAMS; i++) g_csi_params[i] = 0;
    g_csi_nparams = 0;
    g_csi_have_param = 0;
    g_csi_question = 0;
}

static void parser_step(unsigned char c) {
    term2_screen_t *s = ACTIVE();

    switch (g_pstate) {
    case PS_NORMAL:
        if (c == 0x1B) { /* ESC */
            g_pstate = PS_ESC;
            return;
        }
        /* Control characters */
        if (c == '\n') { newline(); return; }
        if (c == '\r') { s->cursor_x = 0; return; }
        if (c == '\b') { if (s->cursor_x > 0) s->cursor_x--; return; }
        if (c == '\t') {
            int next = (s->cursor_x + 4) & ~3;
            if (next >= g_width) { newline(); }
            else                 { s->cursor_x = next; }
            return;
        }
        if (c < 0x20 || c == 0x7F) return;   /* swallow other ctrl chars */
        emit_printable(c);
        return;

    case PS_ESC:
        if (c == '[') {
            g_pstate = PS_CSI;
            parser_reset_csi();
            return;
        }
        if (c == '7') { /* DECSC — save cursor */
            s->saved_cx = s->cursor_x;
            s->saved_cy = s->cursor_y;
            s->saved_palette = s->palette;
            g_pstate = PS_NORMAL;
            return;
        }
        if (c == '8') { /* DECRC — restore cursor */
            s->cursor_x = s->saved_cx;
            s->cursor_y = s->saved_cy;
            s->palette  = s->saved_palette;
            g_pstate = PS_NORMAL;
            return;
        }
        /* Unknown ESC X — ignore */
        g_pstate = PS_NORMAL;
        return;

    case PS_CSI:
        if (c == '?' && !g_csi_have_param && g_csi_nparams == 0) {
            g_csi_question = 1;
            return;
        }
        if (c >= '0' && c <= '9') {
            if (g_csi_nparams == 0) g_csi_nparams = 1;
            g_csi_params[g_csi_nparams - 1] =
                g_csi_params[g_csi_nparams - 1] * 10 + (c - '0');
            g_csi_have_param = 1;
            return;
        }
        if (c == ';') {
            if (g_csi_nparams < CSI_MAX_PARAMS) {
                g_csi_nparams++;
                g_csi_params[g_csi_nparams - 1] = 0;
            }
            g_csi_have_param = 0;
            return;
        }
        if (c >= 0x40 && c <= 0x7E) {
            parser_dispatch_csi(c);
            g_pstate = PS_NORMAL;
            return;
        }
        /* Unexpected — bail */
        g_pstate = PS_NORMAL;
        return;
    }
}

static void parser_dispatch_csi(unsigned char final) {
    term2_screen_t *s = ACTIVE();
    int n, p1, p2;

    /* Private (?-prefixed) sequences */
    if (g_csi_question) {
        int mode = csi_param(0, 0);
        if (mode == 1049) {       /* alt screen with cursor save */
            if (final == 'h') term2_alt_enter();
            else if (final == 'l') term2_alt_leave();
        } else if (mode == 25) {  /* cursor visibility */
            if (final == 'h')      s->cursor_visible = 1;
            else if (final == 'l') s->cursor_visible = 0;
        }
        return;
    }

    switch (final) {
    case 'A': /* CUU — cursor up */
        n = csi_param_or(0, 1);
        s->cursor_y -= n;
        if (s->cursor_y < 0) s->cursor_y = 0;
        return;
    case 'B': /* CUD — cursor down */
        n = csi_param_or(0, 1);
        s->cursor_y += n;
        if (s->cursor_y >= g_height) s->cursor_y = g_height - 1;
        return;
    case 'C': /* CUF — cursor forward */
        n = csi_param_or(0, 1);
        s->cursor_x += n;
        if (s->cursor_x >= g_width) s->cursor_x = g_width - 1;
        return;
    case 'D': /* CUB — cursor back */
        n = csi_param_or(0, 1);
        s->cursor_x -= n;
        if (s->cursor_x < 0) s->cursor_x = 0;
        return;
    case 'H': /* CUP — cursor position (1-based row;col) */
    case 'f':
        p1 = csi_param_or(0, 1);  /* row */
        p2 = csi_param_or(1, 1);  /* col */
        if (p1 < 1) p1 = 1;
        if (p1 > g_height) p1 = g_height;
        if (p2 < 1) p2 = 1;
        if (p2 > g_width)  p2 = g_width;
        s->cursor_y = p1 - 1;
        s->cursor_x = p2 - 1;
        return;
    case 'J': /* ED */
        n = csi_param(0, 0);
        if (n == 0) {
            /* From cursor to end of screen */
            int x;
            for (x = s->cursor_x; x < g_width; x++)
                put_at(x, s->cursor_y, 0, s->palette);
            if (s->cursor_y + 1 <= g_height - 1)
                clear_region(s->cursor_y + 1, g_height - 1, s->palette);
        } else if (n == 1) {
            /* Top to cursor */
            int x;
            if (s->cursor_y > 0)
                clear_region(0, s->cursor_y - 1, s->palette);
            for (x = 0; x <= s->cursor_x; x++)
                put_at(x, s->cursor_y, 0, s->palette);
        } else if (n == 2) {
            clear_region(0, g_height - 1, s->palette);
        }
        return;
    case 'K': /* EL */
        n = csi_param(0, 0);
        {
            int x;
            int xs = 0, xe = g_width - 1;
            if (n == 0)      { xs = s->cursor_x; }
            else if (n == 1) { xe = s->cursor_x; }
            for (x = xs; x <= xe; x++)
                put_at(x, s->cursor_y, 0, s->palette);
        }
        return;
    case 'm': /* SGR — limited support */
        if (g_csi_nparams == 0) {
            s->palette = 0;
            return;
        }
        {
            int i;
            for (i = 0; i < g_csi_nparams; i++) {
                int v = g_csi_params[i];
                if (v == 0)          s->palette = 0;
                else if (v >= 30 && v <= 37) {
                    /* foreground colors map directly to palette idx 0..7 */
                    s->palette = (unsigned char)((s->palette & 0xF8) | (v - 30));
                } else if (v >= 40 && v <= 47) {
                    /* background — map to high nibble (caller convention) */
                    s->palette = (unsigned char)((s->palette & 0x0F) | ((v - 40) << 4));
                } else if (v == 1) {
                    /* bold — bump fg by +8 if there's room */
                    s->palette = (unsigned char)(s->palette | 0x08);
                }
            }
        }
        return;
    case 'r': /* DECSTBM — set scroll region */
        p1 = csi_param_or(0, 1);
        p2 = csi_param_or(1, g_height);
        if (p1 < 1) p1 = 1;
        if (p2 > g_height) p2 = g_height;
        if (p1 < p2) {
            s->scroll_top = p1 - 1;
            s->scroll_bot = p2 - 1;
            s->cursor_x = 0;
            s->cursor_y = s->scroll_top;
        }
        return;
    case 's': /* save cursor */
        s->saved_cx = s->cursor_x;
        s->saved_cy = s->cursor_y;
        s->saved_palette = s->palette;
        return;
    case 'u': /* restore cursor */
        s->cursor_x = s->saved_cx;
        s->cursor_y = s->saved_cy;
        s->palette  = s->saved_palette;
        return;
    case 'S': /* SU — scroll up n lines (no cursor change) */
        scroll_up_in_region(csi_param_or(0, 1));
        return;
    case 'T': /* SD — scroll down */
        scroll_down_in_region(csi_param_or(0, 1));
        return;
    default:
        /* Unsupported — silently ignore */
        return;
    }
}

/* ============================================================== Public ===== */

void term2_init(int width, int height,
                term2_render_cell_fn render_cell,
                term2_uart_mirror_fn uart_mirror) {
    int i;
    if (width  <= 0 || width  > TERM2_MAX_WIDTH)  width  = 40;
    if (height <= 0 || height > TERM2_MAX_HEIGHT) height = 25;
    g_width  = width;
    g_height = height;
    g_render = render_cell;
    g_uart   = uart_mirror;
    g_active = 0;
    g_pstate = PS_NORMAL;
    parser_reset_csi();
    for (i = 0; i < 2; i++) {
        memset(g_screens[i].tiles, 0, sizeof(g_screens[i].tiles));
        memset(g_screens[i].palettes, 0, sizeof(g_screens[i].palettes));
        g_screens[i].cursor_x = 0;
        g_screens[i].cursor_y = 0;
        g_screens[i].saved_cx = 0;
        g_screens[i].saved_cy = 0;
        g_screens[i].palette = 0;
        g_screens[i].saved_palette = 0;
        g_screens[i].scroll_top = 0;
        g_screens[i].scroll_bot = g_height - 1;
        g_screens[i].cursor_visible = 1;
    }
    g_hist_head = 0;
    g_hist_count = 0;
    g_scroll_view_offset = 0;
    /* Paint blank screen */
    {
        int x, y;
        for (y = 0; y < g_height; y++)
            for (x = 0; x < g_width; x++)
                render(x, y, 0, 0);
    }
}

void term2_get_size(int *w, int *h) {
    if (w) *w = g_width;
    if (h) *h = g_height;
}

void term2_write(const char *buf, int len) {
    int i;
    if (!buf) return;
    for (i = 0; i < len; i++)
        parser_step((unsigned char)buf[i]);
}

void term2_putchar(char c) {
    parser_step((unsigned char)c);
}

void term2_puts(const char *s) {
    if (!s) return;
    while (*s) parser_step((unsigned char)*s++);
}

void term2_put_cell(int x, int y, unsigned char tile, unsigned char palette) {
    put_at(x, y, tile, palette);
}

void term2_get_cell(int x, int y, unsigned char *tile, unsigned char *palette) {
    if (x < 0 || x >= g_width || y < 0 || y >= g_height) return;
    if (tile)    *tile    = ACTIVE()->tiles[y][x];
    if (palette) *palette = ACTIVE()->palettes[y][x];
}

void term2_set_cursor(int x, int y) {
    term2_screen_t *s = ACTIVE();
    if (x < 0) x = 0;
    if (x >= g_width)  x = g_width - 1;
    if (y < 0) y = 0;
    if (y >= g_height) y = g_height - 1;
    s->cursor_x = x;
    s->cursor_y = y;
}

void term2_get_cursor(int *x, int *y) {
    if (x) *x = ACTIVE()->cursor_x;
    if (y) *y = ACTIVE()->cursor_y;
}

void term2_set_cursor_visible(int v) { ACTIVE()->cursor_visible = v ? 1 : 0; }
int  term2_get_cursor_visible(void)  { return ACTIVE()->cursor_visible; }

void term2_set_palette(unsigned char p) { ACTIVE()->palette = p; }
unsigned char term2_get_palette(void)   { return ACTIVE()->palette; }

void term2_clear(void) { clear_screen_full(); }

void term2_clear_to_eol(void) {
    term2_screen_t *s = ACTIVE();
    int x;
    for (x = s->cursor_x; x < g_width; x++)
        put_at(x, s->cursor_y, 0, s->palette);
}

void term2_clear_to_eos(void) {
    term2_screen_t *s = ACTIVE();
    term2_clear_to_eol();
    if (s->cursor_y + 1 <= g_height - 1)
        clear_region(s->cursor_y + 1, g_height - 1, s->palette);
}

void term2_set_scroll_region(int top, int bot) {
    term2_screen_t *s = ACTIVE();
    if (top < 0) top = 0;
    if (bot >= g_height) bot = g_height - 1;
    if (top < bot) {
        s->scroll_top = top;
        s->scroll_bot = bot;
    }
}

void term2_scroll_up(int n)   { scroll_up_in_region(n); }
void term2_scroll_down(int n) { scroll_down_in_region(n); }

void term2_alt_enter(void) {
    if (g_active == 1) return;
    /* Save scrollback view position so we can restore on leave */
    g_active = 1;
    /* Reset the alt screen to a clean state, preserving its own cursor/palette
       at defaults but clearing the buffer. */
    {
        term2_screen_t *a = &g_screens[1];
        memset(a->tiles, 0, sizeof(a->tiles));
        memset(a->palettes, 0, sizeof(a->palettes));
        a->cursor_x = 0;
        a->cursor_y = 0;
        a->saved_cx = 0;
        a->saved_cy = 0;
        a->palette = 0;
        a->saved_palette = 0;
        a->scroll_top = 0;
        a->scroll_bot = g_height - 1;
        a->cursor_visible = 1;
    }
    term2_repaint();
}

void term2_alt_leave(void) {
    if (g_active == 0) return;
    g_active = 0;
    /* If user was scrolled back, reset to live (alt screen took over the
       view). The primary buffer state is intact; just repaint live cells. */
    g_scroll_view_offset = 0;
    term2_repaint();
}

int term2_in_alt_screen(void) { return g_active == 1; }

void term2_repaint(void) {
    int x, y;
    term2_screen_t *s = ACTIVE();
    if (g_active == 0 && g_scroll_view_offset > 0) {
        /* Show scrollback */
        for (y = 0; y < g_height; y++) {
            if (y < g_scroll_view_offset) {
                int hist_idx = g_hist_head - g_scroll_view_offset + y;
                while (hist_idx < 0) hist_idx += TERM2_HISTORY_LINES;
                hist_idx %= TERM2_HISTORY_LINES;
                for (x = 0; x < g_width; x++)
                    render(x, y, g_hist_tiles[hist_idx][x], g_hist_pal[hist_idx][x]);
            } else {
                int sy = y - g_scroll_view_offset;
                for (x = 0; x < g_width; x++)
                    render(x, y, s->tiles[sy][x], s->palettes[sy][x]);
            }
        }
    } else {
        for (y = 0; y < g_height; y++)
            for (x = 0; x < g_width; x++)
                render(x, y, s->tiles[y][x], s->palettes[y][x]);
    }
}

int term2_scroll_view_up(void) {
    if (g_active != 0) return 0;        /* alt screen has no history */
    if (g_scroll_view_offset >= g_hist_count) return 0;
    g_scroll_view_offset++;
    term2_repaint();
    return 1;
}

int term2_scroll_view_down(void) {
    if (g_active != 0) return 0;
    if (g_scroll_view_offset == 0) return 0;
    g_scroll_view_offset--;
    term2_repaint();
    return 1;
}

int term2_is_scrolled_back(void) { return g_active == 0 && g_scroll_view_offset > 0; }

void term2_snap_to_bottom(void) {
    if (g_active != 0) return;
    if (g_scroll_view_offset == 0) return;
    g_scroll_view_offset = 0;
    term2_repaint();
}
