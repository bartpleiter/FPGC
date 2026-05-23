/*
 * Host-side unit tests for libterm.
 *
 * Compile:
 *   gcc -O0 -Wall -DTERM_HOST_TEST \
 *       -I Software/C/libfpgc/include \
 *       Tests/host/test_term.c Software/C/libfpgc/term/term.c \
 *       -o /tmp/test_term
 *
 * Run: ./test_term — exits 0 on success, nonzero on first failure.
 */

#include "term.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define WIDTH  40
#define HEIGHT 25

static unsigned char g_render_tiles[HEIGHT][WIDTH];
static unsigned char g_render_pal[HEIGHT][WIDTH];

/* Records every render callback for inspection */
static void render_cb(int x, int y, unsigned char tile, unsigned char palette) {
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) return;
    g_render_tiles[y][x] = tile;
    g_render_pal[y][x]   = palette;
}

static int g_failures = 0;

#define CHECK(cond, msg, ...) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL %s:%d: " msg "\n", __FILE__, __LINE__, ##__VA_ARGS__); \
        g_failures++; \
    } \
} while (0)

static void reset(void) {
    memset(g_render_tiles, 0xFF, sizeof(g_render_tiles));
    memset(g_render_pal, 0xFF, sizeof(g_render_pal));
    term_init(WIDTH, HEIGHT, render_cb, NULL);
}

static void puts_str(const char *s) { term_puts(s); }

/* ---------------------------------------------------------------- */

static void test_basic_print(void) {
    reset();
    puts_str("hello");
    int cx, cy;
    term_get_cursor(&cx, &cy);
    CHECK(cx == 5, "cursor x = %d", cx);
    CHECK(cy == 0, "cursor y = %d", cy);
    CHECK(g_render_tiles[0][0] == 'h', "tile at (0,0)=%d", g_render_tiles[0][0]);
    CHECK(g_render_tiles[0][4] == 'o', "tile at (0,4)=%d", g_render_tiles[0][4]);
}

static void test_newline_wrap(void) {
    reset();
    int i;
    for (i = 0; i < WIDTH; i++) term_putchar('A');
    int cx, cy;
    term_get_cursor(&cx, &cy);
    /* Deferred wrap: cursor stays at last column with pending_wrap set */
    CHECK(cx == WIDTH - 1 && cy == 0, "deferred wrap: cursor=(%d,%d)", cx, cy);
    /* writing one more should trigger the deferred wrap, then land on row 1 col 0 */
    term_putchar('B');
    term_get_cursor(&cx, &cy);
    CHECK(cx == 1 && cy == 1, "after 41st char: cursor=(%d,%d)", cx, cy);
    CHECK(g_render_tiles[1][0] == 'B', "wrap tile=%d", g_render_tiles[1][0]);
}

static void test_scroll_at_bottom(void) {
    reset();
    int row;
    for (row = 0; row < HEIGHT; row++) {
        term_putchar('0' + (row % 10));
        term_putchar('\n');
    }
    /* After HEIGHT newlines, the first row should have scrolled out. */
    /* The very first row pushed to history should have '0'. */
    /* The current top row should now be '1'. */
    CHECK(g_render_tiles[0][0] == '1', "post-scroll row0=%d", g_render_tiles[0][0]);
    /* Scrollback should have one entry. */
    CHECK(term_scroll_view_up() == 1, "should scroll up");
    CHECK(term_is_scrolled_back() == 1, "is_scrolled_back");
    /* New output should snap back to bottom. */
    term_putchar('Z');
    CHECK(term_is_scrolled_back() == 0, "snapped back on output");
}

static void test_csi_cup(void) {
    reset();
    /* CUP to row 5 col 10 (1-based) → (4,9) zero-based */
    puts_str("\033[5;10H");
    int cx, cy;
    term_get_cursor(&cx, &cy);
    CHECK(cx == 9 && cy == 4, "CUP cursor=(%d,%d)", cx, cy);
    puts_str("X");
    CHECK(g_render_tiles[4][9] == 'X', "CUP write");
    /* CUP defaults to home */
    puts_str("\033[H");
    term_get_cursor(&cx, &cy);
    CHECK(cx == 0 && cy == 0, "CUP home=(%d,%d)", cx, cy);
}

static void test_csi_clear(void) {
    reset();
    puts_str("ABCDEFGH");
    /* Move to col 4 (0-based 3), erase to EOL */
    puts_str("\033[1;4H\033[K");
    int cx, cy;
    term_get_cursor(&cx, &cy);
    CHECK(cx == 3 && cy == 0, "after CUP=(%d,%d)", cx, cy);
    /* Cells at col 3..7 should be 0; cells 0..2 should still be ABC */
    CHECK(g_render_tiles[0][0] == 'A', "kept A");
    CHECK(g_render_tiles[0][2] == 'C', "kept C");
    CHECK(g_render_tiles[0][3] == 0, "cleared col3 = %d", g_render_tiles[0][3]);
    CHECK(g_render_tiles[0][7] == 0, "cleared col7 = %d", g_render_tiles[0][7]);
    /* Now CSI 2J clears whole screen */
    puts_str("\033[2J");
    CHECK(g_render_tiles[0][0] == 0, "ED2 row0col0 = %d", g_render_tiles[0][0]);
}

static void test_csi_sgr(void) {
    reset();
    puts_str("\033[31m"); /* ANSI red -> palette 2 (red-on-black) */
    puts_str("R");
    CHECK(g_render_pal[0][0] == 0x02, "SGR31 pal=%d", g_render_pal[0][0]);
    puts_str("\033[32m"); /* ANSI green -> palette 3 (green-on-black) */
    puts_str("G");
    CHECK(g_render_pal[0][1] == 0x03, "SGR32 pal=%d", g_render_pal[0][1]);
    puts_str("\033[0m"); /* reset */
    puts_str("X");
    CHECK(g_render_pal[0][2] == 0x00, "SGR0 pal=%d", g_render_pal[0][2]);
}

static void test_csi_cursor_move(void) {
    reset();
    /* Position then up/down/left/right */
    puts_str("\033[10;10H");
    puts_str("\033[3A"); /* up 3 */
    int cx, cy;
    term_get_cursor(&cx, &cy);
    CHECK(cy == 6, "CUU cy=%d", cy);
    puts_str("\033[2D"); /* left 2 */
    term_get_cursor(&cx, &cy);
    CHECK(cx == 7, "CUB cx=%d", cx);
}

static void test_save_restore_cursor(void) {
    reset();
    puts_str("\033[5;5H");      /* (4,4) */
    puts_str("\033[s");         /* save */
    puts_str("\033[20;20H");    /* (19,19) */
    puts_str("\033[u");         /* restore */
    int cx, cy;
    term_get_cursor(&cx, &cy);
    CHECK(cx == 4 && cy == 4, "save/restore=(%d,%d)", cx, cy);
}

static void test_alt_screen(void) {
    reset();
    /* Write something on primary */
    puts_str("PRIMARY");
    /* Enter alt screen via DEC private mode 1049 */
    puts_str("\033[?1049h");
    CHECK(term_in_alt_screen() == 1, "in alt");
    /* Cell (0,0) should be cleared in alt view */
    /* But render callback was just invoked to repaint — should show 0s */
    CHECK(g_render_tiles[0][0] == 0, "alt clears render to 0, got %d", g_render_tiles[0][0]);
    puts_str("ALT");
    CHECK(g_render_tiles[0][0] == 'A', "alt writes A=%d", g_render_tiles[0][0]);
    /* Leave alt */
    puts_str("\033[?1049l");
    CHECK(term_in_alt_screen() == 0, "back to primary");
    /* Primary should be restored — first cells are PRIMARY again */
    CHECK(g_render_tiles[0][0] == 'P', "primary restored P=%d", g_render_tiles[0][0]);
    CHECK(g_render_tiles[0][6] == 'Y', "primary restored Y=%d", g_render_tiles[0][6]);
}

static void test_cursor_visibility(void) {
    reset();
    puts_str("\033[?25l");
    CHECK(term_get_cursor_visible() == 0, "hide");
    puts_str("\033[?25h");
    CHECK(term_get_cursor_visible() == 1, "show");
}

static void test_scroll_region(void) {
    reset();
    /* Scroll region rows 5..10 (1-based) = 4..9 (0-based) */
    puts_str("\033[5;10r");
    /* Cursor should be at (0,4) */
    int cx, cy;
    term_get_cursor(&cx, &cy);
    CHECK(cx == 0 && cy == 4, "DECSTBM cursor=(%d,%d)", cx, cy);
    /* Fill region to overflow */
    int i;
    for (i = 0; i < 10; i++) {
        term_putchar('X');
        term_putchar('\n');
    }
    /* Cursor should be clamped to bottom of region (y=9) */
    term_get_cursor(&cx, &cy);
    CHECK(cy == 9, "cursor clamped to scroll_bot, got %d", cy);
    /* Row 0 (outside region) should still be empty */
    CHECK(g_render_tiles[0][0] == 0, "row 0 untouched=%d", g_render_tiles[0][0]);
    /* Row 11 (also outside) should be empty */
    CHECK(g_render_tiles[11][0] == 0, "row 11 untouched=%d", g_render_tiles[11][0]);
}

static void test_multiline_block_comment_safety(void) {
    /* Sanity: writing a block of text with embedded newlines doesn't crash */
    reset();
    const char *lines = "line1\nline2\nline3\n";
    term_write(lines, (int)strlen(lines));
    int cx, cy;
    term_get_cursor(&cx, &cy);
    CHECK(cy == 3, "after 3 newlines cy=%d", cy);
}

static void test_tab(void) {
    reset();
    term_putchar('\t');
    int cx, cy;
    term_get_cursor(&cx, &cy);
    CHECK(cx == 4, "tab to col 4, got %d", cx);
    term_putchar('\t');
    term_get_cursor(&cx, &cy);
    CHECK(cx == 8, "tab to col 8, got %d", cx);
}

static void test_backspace(void) {
    reset();
    puts_str("AB");
    term_putchar('\b');
    int cx, cy;
    term_get_cursor(&cx, &cy);
    CHECK(cx == 1, "after \\b cx=%d", cx);
    term_putchar('\b');
    term_putchar('\b'); /* should clamp at 0 */
    term_get_cursor(&cx, &cy);
    CHECK(cx == 0, "clamped cx=%d", cx);
}

static void test_carriage_return(void) {
    reset();
    puts_str("hello");
    term_putchar('\r');
    int cx, cy;
    term_get_cursor(&cx, &cy);
    CHECK(cx == 0 && cy == 0, "after \\r cursor=(%d,%d)", cx, cy);
}

static void test_unknown_csi_swallowed(void) {
    /* An unsupported sequence should not corrupt subsequent output. */
    reset();
    puts_str("\033[99X"); /* unknown final 'X' */
    puts_str("hi");
    CHECK(g_render_tiles[0][0] == 'h', "after unknown CSI, h=%d", g_render_tiles[0][0]);
    CHECK(g_render_tiles[0][1] == 'i', "i=%d", g_render_tiles[0][1]);
}

static void test_ed_modes(void) {
    reset();
    /* Fill three rows */
    int r;
    for (r = 0; r < 3; r++) {
        puts_str("XXXX");
        term_putchar('\n');
    }
    /* Cursor now at (0,3). Move to (0,1) and ED 1 (top to cursor). */
    puts_str("\033[2;1H\033[1J");
    /* Row 0 should be cleared, row 1 column 0 should be cleared, rest of row 1 untouched */
    CHECK(g_render_tiles[0][0] == 0, "row 0 cleared=%d", g_render_tiles[0][0]);
    CHECK(g_render_tiles[1][0] == 0, "row 1 col 0 cleared=%d", g_render_tiles[1][0]);
    CHECK(g_render_tiles[1][3] == 'X', "row 1 col 3 kept=%d", g_render_tiles[1][3]);
    CHECK(g_render_tiles[2][0] == 'X', "row 2 untouched=%d", g_render_tiles[2][0]);
}

/* ---------------------------------------------------------------- */

/* Mock input source: a queue of pre-loaded events. */
#define INPUT_QUEUE_MAX 256
static int input_queue[INPUT_QUEUE_MAX];
static int input_head = 0;
static int input_tail = 0;

static int input_pop(void) {
    if (input_head == input_tail) return -1;
    int ev = input_queue[input_head];
    input_head = (input_head + 1) % INPUT_QUEUE_MAX;
    return ev;
}

static void input_push(int ev) {
    int next = (input_tail + 1) % INPUT_QUEUE_MAX;
    if (next == input_head) return;  /* full */
    input_queue[input_tail] = ev;
    input_tail = next;
}

static void input_reset(void) { input_head = input_tail = 0; }

static void input_push_str(const char *s) {
    while (*s) input_push((unsigned char)*s++);
}

static void test_cooked_basic_line(void) {
    reset();
    input_reset();
    term_set_input_source(input_pop);
    input_push_str("hello\n");
    char buf[64];
    int n = term_read(buf, sizeof(buf), 1);
    CHECK(n == 6, "cooked line len=%d", n);
    CHECK(buf[0] == 'h' && buf[5] == '\n', "buf=%c..%c", buf[0], buf[5]);
    /* Echo should have written 'hello' to screen */
    CHECK(g_render_tiles[0][0] == 'h', "echoed h=%d", g_render_tiles[0][0]);
    CHECK(g_render_tiles[0][4] == 'o', "echoed o=%d", g_render_tiles[0][4]);
}

static void test_cooked_backspace(void) {
    reset();
    input_reset();
    term_set_input_source(input_pop);
    input_push_str("ab");
    input_push(127);    /* backspace */
    input_push('c');
    input_push('\n');
    char buf[64];
    int n = term_read(buf, sizeof(buf), 1);
    CHECK(n == 3, "cooked bs len=%d", n);
    CHECK(buf[0] == 'a' && buf[1] == 'c' && buf[2] == '\n',
          "buf=[%c%c%c]", buf[0], buf[1], buf[2]);
}

static void test_cooked_ctrl_u(void) {
    reset();
    input_reset();
    term_set_input_source(input_pop);
    input_push_str("garbage");
    input_push(21);     /* Ctrl-U */
    input_push_str("ok\n");
    char buf[64];
    int n = term_read(buf, sizeof(buf), 1);
    CHECK(n == 3, "ctrl-u: len=%d", n);
    CHECK(buf[0] == 'o' && buf[1] == 'k' && buf[2] == '\n',
          "ctrl-u buf=[%c%c%c]", buf[0], buf[1], buf[2]);
}

static void test_cooked_ctrl_d_eof(void) {
    reset();
    input_reset();
    term_set_input_source(input_pop);
    input_push(4);      /* Ctrl-D on empty */
    char buf[64];
    int n = term_read(buf, sizeof(buf), 1);
    CHECK(n == 0, "EOF returns 0, got %d", n);
}

static void test_cooked_nonblocking(void) {
    reset();
    input_reset();
    term_set_input_source(input_pop);
    input_push_str("ab"); /* no newline */
    char buf[64];
    int n = term_read(buf, sizeof(buf), 0);
    CHECK(n == 0, "no complete line, got %d", n);
    /* Buffer should still hold 'ab' for next read */
    input_push('\n');
    n = term_read(buf, sizeof(buf), 0);
    CHECK(n == 3, "after \\n, len=%d", n);
}

static void test_cooked_drain_in_chunks(void) {
    reset();
    input_reset();
    term_set_input_source(input_pop);
    input_push_str("abcdefgh\n");
    char buf[4];
    int n = term_read(buf, sizeof(buf), 1);
    CHECK(n == 4, "first chunk=%d", n);
    CHECK(buf[0] == 'a' && buf[3] == 'd', "chunk1");
    n = term_read(buf, sizeof(buf), 0);
    CHECK(n == 4, "second chunk=%d", n);
    CHECK(buf[0] == 'e' && buf[3] == 'h', "chunk2");
    n = term_read(buf, sizeof(buf), 0);
    CHECK(n == 1 && buf[0] == '\n', "remainder len=%d", n);
}

static void test_cooked_no_echo(void) {
    reset();
    input_reset();
    term_set_input_source(input_pop);
    term_set_echo(0);
    input_push_str("hi\n");
    char buf[64];
    term_read(buf, sizeof(buf), 1);
    /* No echo: row 0 col 0 should still be 0 */
    CHECK(g_render_tiles[0][0] == 0, "no-echo: cell=%d", g_render_tiles[0][0]);
}

static void test_raw_event(void) {
    reset();
    input_reset();
    term_set_input_source(input_pop);
    term_set_cooked(0);
    input_push(TERM_KEY_UP);
    input_push('a');
    int ev = term_read_event(0);
    CHECK(ev == TERM_KEY_UP, "first event=%d", ev);
    ev = term_read_event(0);
    CHECK(ev == 'a', "second event=%d", ev);
    ev = term_read_event(0);
    CHECK(ev == -1, "empty=-1, got %d", ev);
}

static void test_raw_read_drops_special(void) {
    reset();
    input_reset();
    term_set_input_source(input_pop);
    term_set_cooked(0);
    input_push('a');
    input_push(TERM_KEY_F1);
    input_push('b');
    char buf[8];
    int n = term_read(buf, sizeof(buf), 0);
    CHECK(n == 2, "raw read len=%d", n);
    CHECK(buf[0] == 'a' && buf[1] == 'b', "raw read=[%c%c]", buf[0], buf[1]);
}

static void test_no_input_source(void) {
    reset();
    /* term_init resets input_pop to NULL. */
    char buf[8];
    int n = term_read(buf, sizeof(buf), 0);
    CHECK(n == -1, "no source = -1, got %d", n);
    int ev = term_read_event(0);
    CHECK(ev == -1, "no source event = -1, got %d", ev);
}

/* ------------- Phase 0: scroll region clamping tests ------------ */

static void test_cuu_clamps_to_scroll_region(void) {
    reset();
    /* Set scroll region to rows 5–20 (1-based) = 4–19 (0-based) */
    puts_str("\033[5;20r");
    /* Cursor is now at (0, 4) = top of region */
    int cx, cy;
    term_get_cursor(&cx, &cy);
    CHECK(cy == 4, "DECSTBM initial cy=%d", cy);
    /* Move cursor down a bit */
    puts_str("\033[3B"); /* CUD 3 → row 7 */
    term_get_cursor(&cx, &cy);
    CHECK(cy == 7, "after CUD cy=%d", cy);
    /* Now CUU 10 should clamp to scroll_top (4), not row 0 */
    puts_str("\033[10A");
    term_get_cursor(&cx, &cy);
    CHECK(cy == 4, "CUU clamp to scroll_top: cy=%d", cy);
}

static void test_cud_clamps_to_scroll_region(void) {
    reset();
    /* Set scroll region to rows 5–20 (1-based) = 4–19 (0-based) */
    puts_str("\033[5;20r");
    /* CUD 50 should clamp to scroll_bot (19), not row 24 */
    puts_str("\033[50B");
    int cx, cy;
    term_get_cursor(&cx, &cy);
    CHECK(cy == 19, "CUD clamp to scroll_bot: cy=%d", cy);
}

static void test_cuu_outside_region_clamps_to_screen(void) {
    reset();
    /* Set scroll region to rows 5–20 */
    puts_str("\033[5;20r");
    /* CUP to row 3 (outside the region, above it) */
    puts_str("\033[3;1H");
    int cx, cy;
    term_get_cursor(&cx, &cy);
    CHECK(cy == 2, "CUP outside region cy=%d", cy);
    /* CUU 10 should clamp to row 0 (screen boundary, not region) */
    puts_str("\033[10A");
    term_get_cursor(&cx, &cy);
    CHECK(cy == 0, "CUU outside region clamps to 0: cy=%d", cy);
}

static void test_cud_outside_region_clamps_to_screen(void) {
    reset();
    /* Set scroll region to rows 5–10 */
    puts_str("\033[5;10r");
    /* CUP to row 22 (outside region, below it) */
    puts_str("\033[22;1H");
    int cx, cy;
    term_get_cursor(&cx, &cy);
    CHECK(cy == 21, "CUP below region cy=%d", cy);
    /* CUD 10 should clamp to row 24 (screen boundary, not region) */
    puts_str("\033[10B");
    term_get_cursor(&cx, &cy);
    CHECK(cy == HEIGHT - 1, "CUD outside region clamps to bottom: cy=%d", cy);
}

/* -------------------- Reverse Index (RI) tests -------------------- */

static void test_ri_mid_screen(void) {
    reset();
    /* Move cursor to row 5 */
    puts_str("\033[6;1H");
    /* RI should just move cursor up */
    puts_str("\033M");
    int cx, cy;
    term_get_cursor(&cx, &cy);
    CHECK(cy == 4, "RI mid-screen cy=%d", cy);
}

static void test_ri_at_scroll_top(void) {
    reset();
    /* Set scroll region rows 5–10 */
    puts_str("\033[5;10r");
    /* Cursor at scroll_top (row 4) */
    int cx, cy;
    term_get_cursor(&cx, &cy);
    CHECK(cy == 4, "at scroll_top cy=%d", cy);
    /* Write content on rows 4–9 */
    puts_str("AAAAAA");
    puts_str("\033[5;1H"); /* back to top of region */
    /* RI should scroll content down (insert blank line at top of region) */
    puts_str("\033M");
    term_get_cursor(&cx, &cy);
    CHECK(cy == 4, "RI at top stays at top: cy=%d", cy);
    /* Row 4 should now be blank (scrolled down) */
    CHECK(g_render_tiles[4][0] == 0, "RI scrolled: row4=%d", g_render_tiles[4][0]);
}

static void test_ri_at_row_zero_no_region(void) {
    reset();
    /* Default scroll region (full screen). Cursor at row 0. */
    puts_str("\033[1;1H");
    puts_str("TOP");
    puts_str("\033[1;1H");
    puts_str("\033M");
    int cx, cy;
    term_get_cursor(&cx, &cy);
    CHECK(cy == 0, "RI at row 0 stays: cy=%d", cy);
    /* Content should have scrolled down, row 0 blank */
    CHECK(g_render_tiles[0][0] == 0, "RI scroll down row0=%d", g_render_tiles[0][0]);
    /* 'TOP' should now be on row 1 */
    CHECK(g_render_tiles[1][0] == 'T', "RI scroll down row1=%d", g_render_tiles[1][0]);
}

/* ------------------- Deferred wrap tests ----------------------- */

static void test_deferred_wrap_40_chars(void) {
    reset();
    /* Write exactly 40 chars — cursor should NOT wrap yet */
    int i;
    for (i = 0; i < WIDTH; i++) term_putchar('X');
    int cx, cy;
    term_get_cursor(&cx, &cy);
    CHECK(cx == WIDTH - 1 && cy == 0, "deferred: cursor=(%d,%d)", cx, cy);
}

static void test_deferred_wrap_41st_char(void) {
    reset();
    int i;
    for (i = 0; i < WIDTH; i++) term_putchar('X');
    /* 41st character should trigger wrap + write on next row */
    term_putchar('Y');
    int cx, cy;
    term_get_cursor(&cx, &cy);
    CHECK(cx == 1 && cy == 1, "41st char: cursor=(%d,%d)", cx, cy);
    CHECK(g_render_tiles[1][0] == 'Y', "41st char tile=%d", g_render_tiles[1][0]);
}

static void test_deferred_wrap_cancelled_by_cup(void) {
    reset();
    int i;
    for (i = 0; i < WIDTH; i++) term_putchar('X');
    /* pending_wrap is set; CUP should cancel it */
    puts_str("\033[1;5H"); /* row 1, col 5 → (4, 0) */
    int cx, cy;
    term_get_cursor(&cx, &cy);
    CHECK(cx == 4 && cy == 0, "CUP cancelled wrap: cursor=(%d,%d)", cx, cy);
    /* Write another char — should NOT wrap */
    term_putchar('Z');
    term_get_cursor(&cx, &cy);
    CHECK(cx == 5 && cy == 0, "after Z: cursor=(%d,%d)", cx, cy);
    CHECK(g_render_tiles[0][4] == 'Z', "Z placed correctly=%d", g_render_tiles[0][4]);
}

static void test_deferred_wrap_cancelled_by_cub(void) {
    reset();
    int i;
    for (i = 0; i < WIDTH; i++) term_putchar('X');
    /* CUB 1 should cancel pending_wrap and move cursor back */
    puts_str("\033[1D");
    int cx, cy;
    term_get_cursor(&cx, &cy);
    CHECK(cx == WIDTH - 2 && cy == 0, "CUB cancelled wrap: cursor=(%d,%d)", cx, cy);
}

static void test_deferred_wrap_bottom_right_no_scroll(void) {
    reset();
    /* Move to last row (row 24, 1-based = row 25) */
    puts_str("\033[25;1H");
    /* Write exactly 40 chars — should NOT scroll */
    int i;
    for (i = 0; i < WIDTH; i++) term_putchar('Z');
    int cx, cy;
    term_get_cursor(&cx, &cy);
    CHECK(cy == HEIGHT - 1, "bottom row no scroll: cy=%d", cy);
    /* Row 0 should be untouched (no scroll happened) */
    CHECK(g_render_tiles[0][0] == 0, "row 0 intact=%d", g_render_tiles[0][0]);
}

static void test_deferred_wrap_bottom_right_scroll_on_41st(void) {
    reset();
    /* Put marker on row 0 */
    puts_str("TOP");
    /* Move to last row */
    puts_str("\033[25;1H");
    /* Write 40 chars + 1 more — the 41st should trigger scroll */
    int i;
    for (i = 0; i < WIDTH; i++) term_putchar('Z');
    term_putchar('!');
    int cx, cy;
    term_get_cursor(&cx, &cy);
    /* After scroll, cursor should be on the last row */
    CHECK(cy == HEIGHT - 1, "after scroll cy=%d", cy);
    /* 'TOP' should have scrolled from row 0 to gone into history */
    /* Row 0 now contains whatever was on row 1 (empty) */
}

static void test_deferred_wrap_newline_fires_wrap(void) {
    reset();
    int i;
    for (i = 0; i < WIDTH; i++) term_putchar('A');
    /* pending_wrap is set. LF should clear pending_wrap and move to next line. */
    term_putchar('\n');
    int cx, cy;
    term_get_cursor(&cx, &cy);
    /* LF subsumes the pending wrap: cursor goes to (0,1) */
    CHECK(cx == 0 && cy == 1, "LF after wrap: cursor=(%d,%d)", cx, cy);
}

/* ---------------------------------------------------------------- */

#define RUN(t) do { printf("  %s\n", #t); t(); } while (0)

int main(void) {
    printf("libterm host tests\n");
    RUN(test_basic_print);
    RUN(test_newline_wrap);
    RUN(test_scroll_at_bottom);
    RUN(test_csi_cup);
    RUN(test_csi_clear);
    RUN(test_csi_sgr);
    RUN(test_csi_cursor_move);
    RUN(test_save_restore_cursor);
    RUN(test_alt_screen);
    RUN(test_cursor_visibility);
    RUN(test_scroll_region);
    RUN(test_multiline_block_comment_safety);
    RUN(test_tab);
    RUN(test_backspace);
    RUN(test_carriage_return);
    RUN(test_unknown_csi_swallowed);
    RUN(test_ed_modes);
    RUN(test_cooked_basic_line);
    RUN(test_cooked_backspace);
    RUN(test_cooked_ctrl_u);
    RUN(test_cooked_ctrl_d_eof);
    RUN(test_cooked_nonblocking);
    RUN(test_cooked_drain_in_chunks);
    RUN(test_cooked_no_echo);
    RUN(test_raw_event);
    RUN(test_raw_read_drops_special);
    RUN(test_no_input_source);
    /* Phase 0: scroll region clamping */
    RUN(test_cuu_clamps_to_scroll_region);
    RUN(test_cud_clamps_to_scroll_region);
    RUN(test_cuu_outside_region_clamps_to_screen);
    RUN(test_cud_outside_region_clamps_to_screen);
    /* Phase 0: reverse index */
    RUN(test_ri_mid_screen);
    RUN(test_ri_at_scroll_top);
    RUN(test_ri_at_row_zero_no_region);
    /* Phase 0: deferred wrap */
    RUN(test_deferred_wrap_40_chars);
    RUN(test_deferred_wrap_41st_char);
    RUN(test_deferred_wrap_cancelled_by_cup);
    RUN(test_deferred_wrap_cancelled_by_cub);
    RUN(test_deferred_wrap_bottom_right_no_scroll);
    RUN(test_deferred_wrap_bottom_right_scroll_on_41st);
    RUN(test_deferred_wrap_newline_fires_wrap);
    printf("\n");
    if (g_failures == 0) {
        printf("OK — all tests passed\n");
        return 0;
    }
    printf("FAILED — %d failure(s)\n", g_failures);
    return 1;
}
