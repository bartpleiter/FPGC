#ifndef FPGC_TERM_H
#define FPGC_TERM_H

/*
 * libterm — terminal abstraction for FPGC BDOS.
 *
 * Design:
 *   - Configurable size (passed to term_init), default 40x25.
 *   - Shadow buffer of (width*height) cells holds tile + palette per cell.
 *   - All output goes through term_write(buf, len) which runs an ANSI
 *     escape-sequence parser and updates the shadow buffer.
 *   - Whenever the shadow buffer changes, a user-supplied callback
 *     (render_cell_fn) is invoked so the caller can push the change
 *     to the GPU. Host tests pass a recording callback; BDOS passes
 *     a thin wrapper around gpu_write_window_tile.
 *   - Alternate screen buffer support: term_alt_enter() saves the
 *     current shadow + cursor + palette state; term_alt_leave() restores
 *     it byte-for-byte. Triggered automatically by ESC[?1049h / ESC[?1049l.
 *   - Scroll regions: ESC[<top>;<bottom>r restricts scrolling to a band.
 *
 * Not in this header (added in Phase A.2):
 *   - Line discipline / input. Phase A.1 is output-only.
 */

#define TERM_MAX_WIDTH      80
#define TERM_MAX_HEIGHT     30
#define TERM_HISTORY_LINES  200

/* Default BDOS terminal dimensions (passed to term_init by the kernel
   bootstrap). */
#define TERM_WIDTH           40
#define TERM_HEIGHT          25
#define TAB_WIDTH            4

/* Render callback: the library calls this whenever a single cell changes.
   Coordinates are 0-based. Caller is expected to push to the GPU. */
typedef void (*term_render_cell_fn)(int x, int y,
                                     unsigned char tile,
                                     unsigned char palette);

/* Optional UART-mirror callback: every printable byte that reaches the
   shadow buffer is also passed to this callback (if non-NULL). Used by
   BDOS to mirror to UART. Escape sequences are NOT forwarded. */
typedef void (*term_uart_mirror_fn)(char c);

/* Input source callback: returns the next event from the keyboard
   FIFO, or -1 if the queue is empty. Event codes are:
     0x00..0x7F  - ASCII (incl. control chars: Ctrl-A=1, Ctrl-C=3, Tab=9,
                   Enter=10, Backspace=127, etc.)
     0x100+      - special keys (arrows, F-keys, Home, etc. — see HID layer)
   The terminal drains this source whenever it needs input; the BDOS
   integration wires it to bdos_keyboard_event_read(). Host tests use a
   queue. */
typedef int (*term_input_pop_fn)(void);

/* Special-key event codes (must match BDOS HID definitions) */
#define TERM_KEY_UP        0x101
#define TERM_KEY_DOWN      0x102
#define TERM_KEY_LEFT      0x103
#define TERM_KEY_RIGHT     0x104
#define TERM_KEY_HOME      0x105
#define TERM_KEY_END       0x106
#define TERM_KEY_INSERT    0x107
#define TERM_KEY_DELETE    0x108
#define TERM_KEY_PGUP      0x109
#define TERM_KEY_PGDN      0x10A
#define TERM_KEY_F1        0x10C
#define TERM_KEY_F12       0x117

/* ---- Lifecycle ---- */
void term_init(int width, int height,
                term_render_cell_fn render_cell,
                term_uart_mirror_fn uart_mirror);
void term_get_size(int *w, int *h);

/* ---- Output ---- */
void term_write(const char *buf, int len);    /* parses ANSI escapes */
void term_putchar(char c);                    /* convenience */
void term_puts(const char *s);                /* convenience */
void term_putint(int value);                  /* signed decimal */
void term_puthex(unsigned int value, int prefix); /* hex, optional "0x" */

/* ---- UART mirror ---- */
void term_set_uart_mirror(int enable);
int  term_get_uart_mirror(void);

/* ---- Direct cell access (bypasses the parser) ---- */
void term_put_cell(int x, int y, unsigned char tile, unsigned char palette);
void term_get_cell(int x, int y, unsigned char *tile, unsigned char *palette);

/* ---- Cursor ---- */
void term_set_cursor(int x, int y);           /* 0-based, clamped */
void term_get_cursor(int *x, int *y);
void term_set_cursor_visible(int visible);
int  term_get_cursor_visible(void);

/* ---- Palette (current SGR state) ---- */
void term_set_palette(unsigned char palette);
unsigned char term_get_palette(void);

/* ---- Screen ops ---- */
void term_clear(void);                        /* clear entire screen */
void term_clear_to_eol(void);
void term_clear_to_eos(void);

/* ---- Scroll region ---- */
void term_set_scroll_region(int top, int bot); /* 0-based, inclusive */
void term_scroll_up(int n);
void term_scroll_down(int n);

/* ---- Alternate screen ---- */
void term_alt_enter(void);
void term_alt_leave(void);
int  term_in_alt_screen(void);

/* ---- Scrollback view (only valid in primary screen) ---- */
int  term_scroll_view_up(void);
int  term_scroll_view_down(void);
int  term_is_scrolled_back(void);
void term_snap_to_bottom(void);

/* ---- Repaint everything via render_cell (used after switching screens) ---- */
void term_repaint(void);

/* ---- Input / line discipline (Phase A.2) ---- */

/* Wire the input source. Pass NULL to disable input. */
void term_set_input_source(term_input_pop_fn pop);

/* Cooked mode (default): term_read() returns line-buffered input with
   simple editing (backspace erases previous char, Ctrl-U erases the line,
   Ctrl-D on empty buffer returns 0/EOF, Enter terminates the line).
   Raw mode: term_read() and term_read_event() return individual events
   immediately. */
void term_set_cooked(int cooked);
int  term_get_cooked(void);

/* Whether to echo user input to the screen in cooked mode. Default: on. */
void term_set_echo(int echo);
int  term_get_echo(void);

/* Cooked-mode read: returns once a full line is available, including the
   terminating '\n' if the buffer is large enough.
     blocking != 0 — spin until a complete line is ready (or EOF).
     blocking == 0 — return 0 immediately if no complete line is available.
   Returns: line length (bytes written to buf), or 0 on EOF / no line ready,
   or -1 if no input source is wired.
   Note: in raw mode this returns up to `max` ASCII bytes; non-ASCII events
   are silently dropped (use term_read_event for raw). */
int term_read(char *buf, int max, int blocking);

/* Raw event read: returns the next input event code (>=0), or -1 if no
   input is available (blocking==0) or no source wired.
   With blocking != 0, spins until an event arrives. */
int term_read_event(int blocking);

#endif /* FPGC_TERM_H */
