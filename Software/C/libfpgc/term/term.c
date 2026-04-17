/*
 * term.c — thin BDOS adapter on top of libterm v2 (term2_*).
 *
 * All terminal state lives in term2.c. This file:
 *   - wires the GPU render callback (gpu_write_window_tile)
 *   - wires the UART mirror callback (uart_putchar) with a runtime toggle
 *   - keeps the legacy term_* function names so the ~800 callers across
 *     BDOS and userBDOS programs continue to work unchanged.
 *
 * New v2-only entry points (alt screen, ANSI escapes, line discipline,
 * input events) are exposed in term.h alongside the legacy ones.
 */

#include "gpu_hal.h"
#include "term.h"
#include "term2.h"
#include "uart.h"
#include <stdio.h>
#include <stddef.h>

static int g_uart_mirror = 1; /* mirror terminal output to UART by default */

static void render_cb(int x, int y, unsigned char tile, unsigned char palette)
{
    gpu_write_window_tile((unsigned int)x, (unsigned int)y, tile, palette);
}

static void uart_cb(char c)
{
    if (g_uart_mirror)
        uart_putchar(c);
}

void term_init(void)
{
    gpu_set_window_palette(0);
    term2_init(TERM_WIDTH, TERM_HEIGHT, render_cb, uart_cb);
}

void term_clear(void) { term2_clear(); }

void term_scroll(void)
{
    /* Legacy explicit "scroll one line" — unused now that wrapping handles it,
       but keep a sane behaviour: scroll the active screen up by one. */
    term2_scroll_up(1);
}

void term_set_cursor(unsigned int x, unsigned int y)
{
    term2_set_cursor((int)x, (int)y);
}

void term_get_cursor(unsigned int *x, unsigned int *y)
{
    int xi = 0, yi = 0;
    term2_get_cursor(&xi, &yi);
    if (x) *x = (unsigned int)xi;
    if (y) *y = (unsigned int)yi;
}

void term_putchar(char c) { term2_putchar(c); }
void term_puts(const char *str) { term2_puts(str); }

void term_putint(int value)
{
    char buf[12];
    snprintf(buf, sizeof(buf), "%d", value);
    term2_puts(buf);
}

void term_puthex(unsigned int value, int prefix)
{
    char buf[12];
    if (prefix) term2_puts("0x");
    snprintf(buf, sizeof(buf), "%x", value);
    term2_puts(buf);
}

void term_write(const char *buf, unsigned int len)
{
    term2_write(buf, (int)len);
}

void term_get_cell(unsigned int x, unsigned int y,
                   unsigned char *tile, unsigned char *palette)
{
    term2_get_cell((int)x, (int)y, tile, palette);
}

void term_put_cell(unsigned int x, unsigned int y,
                   unsigned char tile, unsigned char palette)
{
    term2_put_cell((int)x, (int)y, tile, palette);
}

void term_set_palette(unsigned int palette_index)
{
    term2_set_palette((unsigned char)palette_index);
}

int  term_scroll_view_up(void)    { return term2_scroll_view_up(); }
int  term_scroll_view_down(void)  { return term2_scroll_view_down(); }
int  term_is_scrolled_back(void)  { return term2_is_scrolled_back(); }
void term_set_uart_mirror(int e)  { g_uart_mirror = e; }
#include "gpu_hal.h"
#include "term.h"
#include "uart.h"
#include <stdio.h>
#include <stddef.h>

static int uart_mirror = 1; /* mirror terminal output to UART by default */

void
term_set_uart_mirror(int enable)
{
    uart_mirror = enable;
}

static unsigned int cursor_x = 0;
static unsigned int cursor_y = 0;
static unsigned int current_palette = 0;

/* Shadow buffer (GPU can't read back VRAM) */
static unsigned char screen_tiles[TERM_HEIGHT][TERM_WIDTH];
static unsigned char screen_palettes[TERM_HEIGHT][TERM_WIDTH];

/* Scrollback history ring buffer */
static unsigned char history_tiles[TERM_HISTORY_LINES][TERM_WIDTH];
static unsigned char history_palettes[TERM_HISTORY_LINES][TERM_WIDTH];
static int history_head = 0;
static int history_count = 0;
static int scroll_view_offset = 0;

static void term_advance_cursor(void);
static void term_newline(void);
static void term_scroll_view_refresh(void);
static void term_snap_to_bottom(void);

void
term_init(void)
{
    cursor_x = 0;
    cursor_y = 0;
    current_palette = 0;
    history_head = 0;
    history_count = 0;
    scroll_view_offset = 0;
    gpu_set_window_palette(0);
    term_clear();
}

void
term_clear(void)
{
    unsigned int x, y;
    for (y = 0; y < TERM_HEIGHT; y++) {
        for (x = 0; x < TERM_WIDTH; x++) {
            screen_tiles[y][x] = 0;
            screen_palettes[y][x] = 0;
            gpu_write_window_tile(x, y, 0, 0);
        }
    }
    cursor_x = 0;
    cursor_y = 0;
    scroll_view_offset = 0;
    history_head = 0;
    history_count = 0;
}

void
term_scroll(void)
{
    unsigned int x, y;

    /* Save top row to history */
    for (x = 0; x < TERM_WIDTH; x++) {
        history_tiles[history_head][x] = screen_tiles[0][x];
        history_palettes[history_head][x] = screen_palettes[0][x];
    }
    history_head = (history_head + 1) % TERM_HISTORY_LINES;
    if (history_count < TERM_HISTORY_LINES)
        history_count++;

    if (scroll_view_offset > 0)
        scroll_view_offset = 0;

    /* Shift rows up */
    for (y = 0; y < TERM_HEIGHT - 1; y++) {
        for (x = 0; x < TERM_WIDTH; x++) {
            screen_tiles[y][x] = screen_tiles[y + 1][x];
            screen_palettes[y][x] = screen_palettes[y + 1][x];
            gpu_write_window_tile(x, y, screen_tiles[y][x], screen_palettes[y][x]);
        }
    }

    /* Clear bottom row */
    for (x = 0; x < TERM_WIDTH; x++) {
        screen_tiles[TERM_HEIGHT - 1][x] = 0;
        screen_palettes[TERM_HEIGHT - 1][x] = (unsigned char)current_palette;
        gpu_write_window_tile(x, TERM_HEIGHT - 1, 0, current_palette);
    }
}

void
term_set_cursor(unsigned int x, unsigned int y)
{
    if (x >= TERM_WIDTH)  x = TERM_WIDTH - 1;
    if (y >= TERM_HEIGHT) y = TERM_HEIGHT - 1;
    cursor_x = x;
    cursor_y = y;
}

void
term_get_cursor(unsigned int *x, unsigned int *y)
{
    if (x) *x = cursor_x;
    if (y) *y = cursor_y;
}

static void
term_advance_cursor(void)
{
    cursor_x++;
    if (cursor_x >= TERM_WIDTH) {
        cursor_x = 0;
        cursor_y++;
        if (cursor_y >= TERM_HEIGHT) {
            term_scroll();
            cursor_y = TERM_HEIGHT - 1;
        }
    }
}

static void
term_newline(void)
{
    cursor_x = 0;
    cursor_y++;
    if (cursor_y >= TERM_HEIGHT) {
        term_scroll();
        cursor_y = TERM_HEIGHT - 1;
    }
}

void
term_putchar(char c)
{
    if (uart_mirror)
        uart_putchar(c);

    if (scroll_view_offset > 0)
        term_snap_to_bottom();

    if (c == '\n') {
        term_newline();
        return;
    } else if (c == '\r') {
        cursor_x = 0;
        return;
    } else if (c == '\t') {
        cursor_x = (cursor_x + TAB_WIDTH) & ~(TAB_WIDTH - 1);
        if (cursor_x >= TERM_WIDTH)
            term_newline();
        return;
    } else if (c == '\b') {
        if (cursor_x > 0)
            cursor_x--;
        return;
    }

    screen_tiles[cursor_y][cursor_x] = (unsigned char)c;
    screen_palettes[cursor_y][cursor_x] = (unsigned char)current_palette;
    gpu_write_window_tile(cursor_x, cursor_y, (unsigned char)c, current_palette);
    term_advance_cursor();
}

void
term_puts(const char *str)
{
    if (!str) return;
    while (*str)
        term_putchar(*str++);
}

void
term_putint(int value)
{
    char buf[12];
    snprintf(buf, sizeof(buf), "%d", value);
    term_puts(buf);
}

void
term_puthex(unsigned int value, int prefix)
{
    char buf[12];
    if (prefix) term_puts("0x");
    snprintf(buf, sizeof(buf), "%x", value);
    term_puts(buf);
}

void
term_write(const char *buf, unsigned int len)
{
    unsigned int i;
    if (!buf) return;
    for (i = 0; i < len; i++)
        term_putchar(buf[i]);
}

void
term_get_cell(unsigned int x, unsigned int y, unsigned char *tile, unsigned char *palette)
{
    if (x >= TERM_WIDTH || y >= TERM_HEIGHT)
        return;
    if (tile)    *tile    = screen_tiles[y][x];
    if (palette) *palette = screen_palettes[y][x];
}

void
term_put_cell(unsigned int x, unsigned int y, unsigned char tile, unsigned char palette)
{
    if (x >= TERM_WIDTH || y >= TERM_HEIGHT)
        return;
    screen_tiles[y][x] = tile;
    screen_palettes[y][x] = palette;
    gpu_write_window_tile(x, y, tile, palette);
}

void
term_set_palette(unsigned int palette_index)
{
    current_palette = palette_index;
}

static void
term_scroll_view_refresh(void)
{
    int y, hist_idx;
    unsigned int x;

    for (y = 0; y < (int)TERM_HEIGHT; y++) {
        if (y < scroll_view_offset) {
            hist_idx = history_head - scroll_view_offset + y;
            if (hist_idx < 0)
                hist_idx += TERM_HISTORY_LINES;
            for (x = 0; x < TERM_WIDTH; x++)
                gpu_write_window_tile(x, (unsigned int)y,
                    history_tiles[hist_idx][x], history_palettes[hist_idx][x]);
        } else {
            for (x = 0; x < TERM_WIDTH; x++)
                gpu_write_window_tile(x, (unsigned int)y,
                    screen_tiles[y - scroll_view_offset][x],
                    screen_palettes[y - scroll_view_offset][x]);
        }
    }
}

static void
term_snap_to_bottom(void)
{
    unsigned int x, y;
    scroll_view_offset = 0;
    for (y = 0; y < TERM_HEIGHT; y++)
        for (x = 0; x < TERM_WIDTH; x++)
            gpu_write_window_tile(x, y, screen_tiles[y][x], screen_palettes[y][x]);
}

int
term_scroll_view_up(void)
{
    if (scroll_view_offset < history_count) {
        scroll_view_offset++;
        term_scroll_view_refresh();
        return 1;
    }
    return 0;
}

int
term_scroll_view_down(void)
{
    if (scroll_view_offset > 0) {
        scroll_view_offset--;
        term_scroll_view_refresh();
        return 1;
    }
    return 0;
}

int
term_is_scrolled_back(void)
{
    return scroll_view_offset > 0 ? 1 : 0;
}
