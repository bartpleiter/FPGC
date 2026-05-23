# Terminal (libterm)

`libterm` is the terminal layer that backs `/dev/tty` in
[BDOS](OS.md). It owns *all* terminal state — the on-screen cell grid,
cursor, palettes, scrollback ring, alternate-screen save area, and the
ANSI escape parser. Everything else (the shell, user programs, libc
`printf`) talks to it indirectly by writing bytes into fd 1.

It lives in two files:

- `Software/C/libfpgc/term/term.c` (~980 LOC) — implementation.
- `Software/C/libfpgc/include/term.h` — public API.

The legacy `term.c` shim was deleted in Phase E; all callers now use the
`term_*` symbols directly.

## Architecture

```
+----------------------+      +-------------------------+
| user program         | ---> | sys_write(1, buf, len)  |
+----------------------+      +-------------------------+
                                          |
                                          v
                              +-------------------------+
                              |   BDOS VFS  /dev/tty    |
                              +-------------------------+
                                          |
                                          v
                              +-------------------------+
                              |   libterm parser     |
                              |   (term_write)         |
                              +-------------------------+
                                |          |
                                v          v
                       +---------------+  +-------------+
                       | tile renderer |  | UART mirror |
                       | (gpu_write_   |  | (uart_putc) |
                       |  window_tile) |  +-------------+
                       +---------------+
```

`term_init()` is called once during `kernel_init` (`Software/C/kernel/src/init.c`)
with two callbacks: a tile renderer (currently `gpu_write_window_tile`) and
a UART mirror (currently `uart_putchar`). Disabling the UART mirror at
runtime is a single call to `term_set_uart_mirror(0)`.

## ANSI escape support

libterm implements a useful subset of VT100/xterm escapes — enough for
ncurses-style fullscreen apps and conventional terminal output:

### CSI sequences (`ESC [ ...`)

| Sequence | Name | Behaviour |
|----------|------|-----------|
| `[ Pn A` / `B` / `C` / `D` | CUU/CUD/CUF/CUB | Cursor up / down / forward / back (Pn defaults to 1) |
| `[ Pl ; Pc H` / `f` | CUP / HVP | Cursor position (1-based row;col) |
| `[ Pn G` | CHA | Cursor horizontal absolute |
| `[ Pn d` | VPA | Cursor vertical absolute |
| `[ Pn J` | ED | Erase display (`0` = cursor→end, `1` = start→cursor, `2` = whole screen) |
| `[ Pn K` | EL | Erase in line (same `0`/`1`/`2` semantics) |
| `[ Pn ; Pn r` | DECSTBM | Set top/bottom scroll region |
| `[ Pn S` / `T` | SU / SD | Scroll up / down within the region |
| `[ Pn m` | SGR | Palette / attribute select (see below) |
| `[ s` / `[ u` | SCP / RCP | Save / restore cursor + palette |
| `[ ? 25 h` / `l` | DECTCEM | Show / hide cursor |
| `[ ? 7 h` / `l` | DECAWM | Auto-wrap on (default) / off. With wrap off, writing to the last column clamps the cursor there instead of advancing — useful for full-screen apps that draw to the bottom-right cell without triggering a scroll. |
| `[ ? 1049 h` / `l` | xterm alt screen | Push / pop the entire screen + cursor |

### Single-byte controls

`\b`, `\t` (tab width = 4), `\r`, `\n` are honoured. Tabs round up to the
next multiple of 4 columns. `\n` is a true line feed (no implicit CR).

### SGR palette mapping

The FPGC has a tile-palette per cell rather than separate fg/bg attributes.
SGR is mapped pragmatically:

- `\x1b[0m` resets the palette to 0.
- `\x1b[30m`..`\x1b[37m` selects palette indices 0..7 in the low nibble
  (foreground-style colors).
- `\x1b[40m`..`\x1b[47m` writes 0..7 into the high nibble
  (background-style colors).
- `\x1b[1m` (bold) sets the `0x08` bit in the low nibble — useful as a
  "bright" toggle when combined with the above.

If you need exact control of the underlying palette, draw with
`term_put_cell(x, y, tile, palette)` directly inside the kernel — there
is no userland syscall for raw palette writes any more (one of the
removals in Phase E).

## Public API (selected)

```c
void term_init(int width, int height,
                term_render_cell_fn render_cell,
                term_uart_mirror_fn uart_mirror);

/* High-level character / string output. Goes through the parser. */
void term_putchar(char c);
void term_puts   (const char *s);
void term_write  (const char *buf, int len);

/* Convenience formatters used by BDOS panic / boot output. */
void term_putint (int v);
void term_puthex (unsigned int v, int prefix); /* hex, with optional "0x" prefix */

/* Direct cell access (bypasses the parser; for kernel use). */
void term_put_cell(int x, int y, unsigned char tile, unsigned char palette);
void term_get_cell(int x, int y, unsigned char *tile, unsigned char *palette);

/* Cursor + display state. */
void term_set_cursor(int x, int y);
void term_get_cursor(int *x, int *y);
void term_set_cursor_visible(int visible);
int  term_get_cursor_visible(void);
void term_clear(void);
void term_clear_to_eol(void);
void term_clear_to_eos(void);
void term_get_size(int *w, int *h);

/* Scroll region + scrolling. */
void term_set_scroll_region(int top, int bot);
void term_scroll_up(int n);
void term_scroll_down(int n);

/* Alternate screen. */
void term_alt_enter(void);
void term_alt_leave(void);
int  term_in_alt_screen(void);

/* Scrollback view. */
int  term_scroll_view_up(void);   /* returns 1 if scrolled, 0 if at top */
int  term_scroll_view_down(void); /* returns 1 if scrolled, 0 if at bottom */
int  term_is_scrolled_back(void);
void term_snap_to_bottom(void);

/* Repaint entire screen via render_cell callback. */
void term_repaint(void);

/* Misc. */
void term_set_palette       (unsigned char palette);
unsigned char term_get_palette(void);
void term_set_uart_mirror   (int enabled);
int  term_get_uart_mirror   (void);

/* Input / line discipline. */
void term_set_input_source(term_input_pop_fn pop);
void term_set_cooked(int cooked);
int  term_get_cooked(void);
void term_set_echo(int echo);
int  term_get_echo(void);
int  term_read(char *buf, int max, int blocking);
```

`TERM_WIDTH`, `TERM_HEIGHT`, `TERM_HISTORY_LINES`, and `TAB_WIDTH` are
provided as `#define`s in `term.h` for code that wants compile-time
constants.

## Raw input — `/dev/tty` event packets

For games and other programs that need single-key responsiveness rather
than line-buffered input, open `/dev/tty` with the `O_RAW` flag (and
typically `O_NONBLOCK` too):

```c
int fd = sys_tty_open_raw(1 /* nonblocking */);
unsigned char buf[4];
while (sys_read(fd, buf, 4) == 4) {
    int code = buf[0]
             | (buf[1] <<  8)
             | (buf[2] << 16)
             | (buf[3] << 24);
    /* code is ASCII for printable keys, or KEY_UP / KEY_F1 / ... for
       special keys; see <syscall.h> for the constants. */
}
```

`Software/C/userBDOS/snake.c` is the in-tree reference port for this API.

In cooked mode (no `O_RAW`), reads from `/dev/tty` block until a full line
has been entered and return that line as bytes — exactly what the shell
line editor and `sys_read(0, ...)` users expect.

## See also

- [OS.md — VFS / process model](OS.md#virtual-file-system-vfs)
- [Shell.md — shell syntax / built-ins](Shell.md)
