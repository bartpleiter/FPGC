#ifndef FPGC_TERM2_H
#define FPGC_TERM2_H

/*
 * libterm v2 — terminal abstraction for FPGC BDOS.
 *
 * Design:
 *   - Configurable size (passed to term2_init), default 40x25.
 *   - Shadow buffer of (width*height) cells holds tile + palette per cell.
 *   - All output goes through term2_write(buf, len) which runs an ANSI
 *     escape-sequence parser and updates the shadow buffer.
 *   - Whenever the shadow buffer changes, a user-supplied callback
 *     (render_cell_fn) is invoked so the caller can push the change
 *     to the GPU. Host tests pass a recording callback; BDOS passes
 *     a thin wrapper around gpu_write_window_tile.
 *   - Alternate screen buffer support: term2_alt_enter() saves the
 *     current shadow + cursor + palette state; term2_alt_leave() restores
 *     it byte-for-byte. Triggered automatically by ESC[?1049h / ESC[?1049l.
 *   - Scroll regions: ESC[<top>;<bottom>r restricts scrolling to a band.
 *
 * Not in this header (added in Phase A.2):
 *   - Line discipline / input. Phase A.1 is output-only.
 */

#define TERM2_MAX_WIDTH      80
#define TERM2_MAX_HEIGHT     30
#define TERM2_HISTORY_LINES  200

/* Render callback: the library calls this whenever a single cell changes.
   Coordinates are 0-based. Caller is expected to push to the GPU. */
typedef void (*term2_render_cell_fn)(int x, int y,
                                     unsigned char tile,
                                     unsigned char palette);

/* Optional UART-mirror callback: every printable byte that reaches the
   shadow buffer is also passed to this callback (if non-NULL). Used by
   BDOS to mirror to UART. Escape sequences are NOT forwarded. */
typedef void (*term2_uart_mirror_fn)(char c);

/* ---- Lifecycle ---- */
void term2_init(int width, int height,
                term2_render_cell_fn render_cell,
                term2_uart_mirror_fn uart_mirror);
void term2_get_size(int *w, int *h);

/* ---- Output ---- */
void term2_write(const char *buf, int len);    /* parses ANSI escapes */
void term2_putchar(char c);                    /* convenience */
void term2_puts(const char *s);                /* convenience */

/* ---- Direct cell access (bypasses the parser) ---- */
void term2_put_cell(int x, int y, unsigned char tile, unsigned char palette);
void term2_get_cell(int x, int y, unsigned char *tile, unsigned char *palette);

/* ---- Cursor ---- */
void term2_set_cursor(int x, int y);           /* 0-based, clamped */
void term2_get_cursor(int *x, int *y);
void term2_set_cursor_visible(int visible);
int  term2_get_cursor_visible(void);

/* ---- Palette (current SGR state) ---- */
void term2_set_palette(unsigned char palette);
unsigned char term2_get_palette(void);

/* ---- Screen ops ---- */
void term2_clear(void);                        /* clear entire screen */
void term2_clear_to_eol(void);
void term2_clear_to_eos(void);

/* ---- Scroll region ---- */
void term2_set_scroll_region(int top, int bot); /* 0-based, inclusive */
void term2_scroll_up(int n);
void term2_scroll_down(int n);

/* ---- Alternate screen ---- */
void term2_alt_enter(void);
void term2_alt_leave(void);
int  term2_in_alt_screen(void);

/* ---- Scrollback view (only valid in primary screen) ---- */
int  term2_scroll_view_up(void);
int  term2_scroll_view_down(void);
int  term2_is_scrolled_back(void);
void term2_snap_to_bottom(void);

/* ---- Repaint everything via render_cell (used after switching screens) ---- */
void term2_repaint(void);

#endif /* FPGC_TERM2_H */
