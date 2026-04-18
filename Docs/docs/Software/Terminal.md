# Terminal (libterm v2)

`libterm v2` is the terminal layer that backs `/dev/tty` in
[BDOS](OS.md). It owns *all* terminal state — the on-screen cell grid,
cursor, palettes, scrollback ring, alternate-screen save area, and the
ANSI escape parser. Everything else (the shell, user programs, libc
`printf`) talks to it indirectly by writing bytes into fd 1.

It lives in two files:

- `Software/C/libfpgc/term/term2.c` (~870 LOC) — implementation.
- `Software/C/libfpgc/include/term2.h` — public API.

The legacy `term.c` shim was deleted in Phase E; all callers now use the
`term2_*` symbols directly.

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
                              |   libterm v2 parser     |
                              |   (term2_write)         |
                              +-------------------------+
                                |          |
                                v          v
                       +---------------+  +-------------+
                       | tile renderer |  | UART mirror |
                       | (gpu_write_   |  | (uart_putc) |
                       |  window_tile) |  +-------------+
                       +---------------+
```

`term2_init()` is called once during `bdos_init` (`Software/C/bdos/init.c`)
with two callbacks: a tile renderer (currently `gpu_write_window_tile`) and
a UART mirror (currently `uart_putchar`). Disabling the UART mirror at
runtime is a single call to `term2_set_uart_mirror(0)`.

## ANSI escape support

libterm v2 implements a useful subset of VT100/xterm escapes — enough for
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
`term2_put_cell(x, y, tile, palette)` directly inside the kernel — there
is no userland syscall for raw palette writes any more (one of the
removals in Phase E).

## Public API (selected)

```c
void term2_init(int width, int height,
                term2_render_cb_t render,
                term2_uart_cb_t uart);

/* High-level character / string output. Goes through the parser. */
void term2_putchar(int c);
void term2_puts   (const char *s);
void term2_write  (const char *buf, int len);

/* Convenience formatters used by BDOS panic / boot output. */
void term2_putint (int v);
void term2_puthex (unsigned int v);

/* Direct cell access (bypasses the parser; for kernel use). */
void term2_put_cell(int x, int y, unsigned char tile, unsigned char palette);
void term2_get_cell(int x, int y, unsigned char *tile, unsigned char *palette);

/* Cursor + display state. */
void term2_set_cursor(int x, int y);
void term2_get_cursor(int *x, int *y);
int  term2_get_cursor_visible(void);
void term2_clear(void);

/* Scrollback view. */
void term2_scroll_view_up(int lines);
void term2_scroll_view_down(int lines);
int  term2_is_scrolled_back(void);

/* Misc. */
void term2_set_palette       (int index);
void term2_set_uart_mirror   (int enabled);
int  term2_get_uart_mirror   (void);
```

`TERM_WIDTH`, `TERM_HEIGHT`, `TERM_HISTORY_LINES`, and `TAB_WIDTH` are
provided as `#define`s in `term2.h` for code that wants compile-time
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
- [shell-terminal-v2 plan](../../plans/shell-terminal-v2.md)
