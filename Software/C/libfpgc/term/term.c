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
