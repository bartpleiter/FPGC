/*
 * 2048.c — Classic 2048 puzzle game for FPGC BDOS.
 *
 * Terminal-based (libterm / ANSI escapes), following the snake.c
 * reference port pattern.
 *
 * Controls: arrow keys / WASD to slide, R to restart, Q / Esc to quit.
 */

#include <syscall.h>

/* ---- Timing ---- */
#define TICK_DELAY_MS  10

/* ---- Board ---- */
#define GRID_ROWS  4
#define GRID_COLS  4
#define CELL_W     3
#define CELL_H     1
#define WIN_TILE   2048

/* ---- Layout (40x25 terminal) ----
 * Each cell: 2 rows tall (content + blank), borders between cells.
 * Grid: 4 cells x (3+1) = 16 chars wide, 5 borders + 8 cell rows = 13 rows
 * Rows: 1 (title), 3-15 (grid), 17 (score/best), 18 (msg), 19-21 (hints)
 * Centered: (40 - 17) / 2 = 11 start column
 */
#define GRID_START_X   11
#define TITLE_Y        1
#define GRID_TOP_Y     3
#define SCORE_Y        17
#define MSG_Y         18
#define HINT1_Y       19
#define HINT2_Y       20
#define HINT3_Y       21

/* ---- Directions ---- */
#define DIR_LEFT  0
#define DIR_RIGHT 1
#define DIR_UP    2
#define DIR_DOWN  3

/* ---- Tile colors ----
 * FPGC term: \x1b[48;5;Nm sets the full palette byte to N.
 * Standard SGR 30-37 override the palette entirely, so we use
 * unsupported SGR 90-97 for fg (ignored by terminal, palette stays).
 * Palette 235 = dark gray (empty cells).
 * Palette 1 = black-on-white (numbered cells, from GPU default palette).
 */
static const char *tile_bg_colors[] = {
    [0]     = "\x1b[48;5;235m",  /* empty: dark gray */
    [1]     = "\x1b[48;5;1m",    /* 2: black-on-white */
    [2]     = "\x1b[48;5;1m",    /* 4: black-on-white */
    [3]     = "\x1b[48;5;1m",    /* 8: black-on-white */
    [4]     = "\x1b[48;5;1m",    /* 16: black-on-white */
    [5]     = "\x1b[48;5;1m",    /* 32: black-on-white */
    [6]     = "\x1b[48;5;1m",    /* 64: black-on-white */
    [7]     = "\x1b[48;5;1m",    /* 128: black-on-white */
    [8]     = "\x1b[48;5;1m",    /* 256: black-on-white */
    [9]     = "\x1b[48;5;1m",    /* 512: black-on-white */
    [10]    = "\x1b[48;5;1m",    /* 1024: black-on-white */
    [11]    = "\x1b[48;5;1m",    /* 2048: black-on-white */
    [12]    = "\x1b[48;5;1m",    /* 4096+: black-on-white */
};

static const char *tile_fg_colors[] = {
    [0]     = "\x1b[90m",   /* empty: ignored (palette stays 235) */
    [1]     = "\x1b[90m",   /* 2: ignored (palette stays 1) */
    [2]     = "\x1b[90m",   /* 4: ignored (palette stays 1) */
    [3]     = "\x1b[90m",   /* 8: ignored (palette stays 1) */
    [4]     = "\x1b[90m",   /* 16: ignored (palette stays 1) */
    [5]     = "\x1b[90m",   /* 32: ignored (palette stays 1) */
    [6]     = "\x1b[90m",   /* 64: ignored (palette stays 1) */
    [7]     = "\x1b[90m",   /* 128: ignored (palette stays 1) */
    [8]     = "\x1b[90m",   /* 256: ignored (palette stays 1) */
    [9]     = "\x1b[90m",   /* 512: ignored (palette stays 1) */
    [10]    = "\x1b[90m",   /* 1024: ignored (palette stays 1) */
    [11]    = "\x1b[90m",   /* 2048: ignored (palette stays 1) */
    [12]    = "\x1b[90m",   /* 4096+: ignored (palette stays 1) */
};

/* ---- State ---- */
static int board[GRID_ROWS][GRID_COLS];
static int score;
static int best_score;
static int won_game;
static int game_over;
static int tty_fd;

/* ---- RNG (LFSR, same pattern as snake.c) ---- */
static unsigned int rng_lfsr;

static int rng_rand(void)
{
    int bit;
    bit = ((rng_lfsr >> 0) ^ (rng_lfsr >> 2) ^
           (rng_lfsr >> 3) ^ (rng_lfsr >> 5)) & 1;
    rng_lfsr = (rng_lfsr >> 1) | (bit << 15);
    return (int)rng_lfsr;
}

static int rng_mod(int val, int modulus)
{
    if (val < 0) val = -val;
    return val % modulus;
}

/* ---- Output helpers ---- */

static int write_str(const char *s)
{
    int n = 0;
    while (s[n]) n++;
    return sys_write(1, s, n);
}

static int append_uint(char *out, int len, int val)
{
    char tmp[12];
    int i = 0;
    if (val == 0) {
        tmp[i++] = '0';
    } else {
        while (val > 0) {
            tmp[i++] = (char)('0' + (val % 10));
            val /= 10;
        }
    }
    while (i > 0) {
        out[len++] = tmp[--i];
    }
    return len;
}

static void cursor_to(int x, int y)
{
    char buf[16];
    int n = 0;
    buf[n++] = '\x1b';
    buf[n++] = '[';
    n = append_uint(buf, n, y + 1);
    buf[n++] = ';';
    n = append_uint(buf, n, x + 1);
    buf[n++] = 'H';
    sys_write(1, buf, n);
}

static void clear_screen(void)
{
    write_str("\x1b[2J\x1b[H");
}

static void reset_colors(void)
{
    write_str("\x1b[0m");
}

/* ---- Board helpers ---- */

static void clear_board(void)
{
    int r, c;
    for (r = 0; r < GRID_ROWS; r++)
        for (c = 0; c < GRID_COLS; c++)
            board[r][c] = 0;
}

static int count_empty(void)
{
    int r, c, count = 0;
    for (r = 0; r < GRID_ROWS; r++)
        for (c = 0; c < GRID_COLS; c++)
            if (board[r][c] == 0) count++;
    return count;
}

static void place_random_tile(void)
{
    int empty = count_empty();
    if (empty == 0) return;

    int idx = rng_mod(rng_rand(), empty);
    int r, c, count = 0;

    for (r = 0; r < GRID_ROWS; r++) {
        for (c = 0; c < GRID_COLS; c++) {
            if (board[r][c] == 0) {
                if (count == idx) {
                    board[r][c] = (rng_mod(rng_rand(), 10) == 0) ? 4 : 2;
                    return;
                }
                count++;
            }
        }
    }
}

static int tile_color_index(int val)
{
    if (val == 0)   return 0;
    if (val == 2)   return 1;
    if (val == 4)   return 2;
    if (val == 8)   return 3;
    if (val == 16)  return 4;
    if (val == 32)  return 5;
    if (val == 64)  return 6;
    if (val == 128) return 7;
    if (val == 256) return 8;
    if (val == 512) return 9;
    if (val == 1024) return 10;
    if (val == 2048) return 11;
    return 12;  /* 4096+ */
}

/* ---- Slide operations ---- */

static int slide_row_left(int row[4])
{
    int orig[4];
    int compact[4];
    int merged[4];
    int i, j;

    orig[0] = row[0]; orig[1] = row[1];
    orig[2] = row[2]; orig[3] = row[3];

    /* Step 1: compact non-zero */
    j = 0;
    for (i = 0; i < 4; i++) {
        if (row[i] != 0) compact[j++] = row[i];
    }
    while (j < 4) compact[j++] = 0;

    /* Step 2: merge adjacent equals */
    j = 0;
    for (i = 0; i < 4; i++) merged[i] = 0;
    for (i = 0; i < 4; i++) {
        if (compact[i] != 0) {
            if (j > 0 && merged[j-1] == compact[i]) {
                merged[j-1] *= 2;
                score += merged[j-1];
            } else {
                if (j < 4) merged[j++] = compact[i];
            }
        }
    }
    while (j < 4) merged[j++] = 0;

    /* Step 3: write back and check changed */
    int changed = 0;
    for (i = 0; i < 4; i++) {
        row[i] = merged[i];
        if (row[i] != orig[i]) changed = 1;
    }
    return changed;
}

/* Slide entire board in a direction. Returns 1 if board changed. */
static int slide_board(int direction)
{
    int r, c, changed = 0;
    int row[4];

    if (direction == DIR_LEFT) {
        for (r = 0; r < GRID_ROWS; r++) {
            row[0] = board[r][0]; row[1] = board[r][1];
            row[2] = board[r][2]; row[3] = board[r][3];
            if (slide_row_left(row)) changed = 1;
            board[r][0] = row[0]; board[r][1] = row[1];
            board[r][2] = row[2]; board[r][3] = row[3];
        }
    } else if (direction == DIR_RIGHT) {
        for (r = 0; r < GRID_ROWS; r++) {
            row[0] = board[r][3]; row[1] = board[r][2];
            row[2] = board[r][1]; row[3] = board[r][0];
            if (slide_row_left(row)) changed = 1;
            board[r][3] = row[0]; board[r][2] = row[1];
            board[r][1] = row[2]; board[r][0] = row[3];
        }
    } else if (direction == DIR_UP) {
        for (c = 0; c < GRID_COLS; c++) {
            row[0] = board[0][c]; row[1] = board[1][c];
            row[2] = board[2][c]; row[3] = board[3][c];
            if (slide_row_left(row)) changed = 1;
            board[0][c] = row[0]; board[1][c] = row[1];
            board[2][c] = row[2]; board[3][c] = row[3];
        }
    } else if (direction == DIR_DOWN) {
        for (c = 0; c < GRID_COLS; c++) {
            row[0] = board[3][c]; row[1] = board[2][c];
            row[2] = board[1][c]; row[3] = board[0][c];
            if (slide_row_left(row)) changed = 1;
            board[3][c] = row[0]; board[2][c] = row[1];
            board[1][c] = row[2]; board[0][c] = row[3];
        }
    }
    return changed;
}

/* Check if any valid moves remain */
static int can_move(void)
{
    int r, c;

    /* Any empty cell means we can move */
    for (r = 0; r < GRID_ROWS; r++)
        for (c = 0; c < GRID_COLS; c++)
            if (board[r][c] == 0) return 1;

    /* Any adjacent equal pair means we can merge */
    for (r = 0; r < GRID_ROWS; r++) {
        for (c = 0; c < GRID_COLS; c++) {
            if (c + 1 < GRID_COLS && board[r][c] == board[r][c + 1])
                return 1;
            if (r + 1 < GRID_ROWS && board[r][c] == board[r + 1][c])
                return 1;
        }
    }
    return 0;
}

/* Check if 2048 tile exists */
static int check_win(void)
{
    int r, c;
    for (r = 0; r < GRID_ROWS; r++)
        for (c = 0; c < GRID_COLS; c++)
            if (board[r][c] == WIN_TILE) return 1;
    return 0;
}

/* ---- Rendering ---- */

static void draw_title(void)
{
    cursor_to(GRID_START_X + 6, TITLE_Y);
    write_str("\x1b[1m  2048  \x1b[0m");
}

static void draw_grid_top_border(void)
{
    char buf[32];
    int i, n = 0;
    for (i = 0; i < GRID_COLS; i++) {
        buf[n++] = '+';
        int w = CELL_W;
        while (w > 0) { buf[n++] = '-'; w--; }
    }
    buf[n++] = '+';
    buf[n] = '\0';
    sys_write(1, buf, n);
}

static void draw_cell_content(int row_idx)
{
    int c;
    char numbuf[8];
    /* Each cell is 2 rows tall. Content on first row, blank on second. */
    int y = GRID_TOP_Y + 1 + row_idx * 3;

    /* --- Row 1: content --- */
    cursor_to(GRID_START_X, y);
    write_str("|");

    for (c = 0; c < GRID_COLS; c++) {
        int val = board[row_idx][c];
        int ci = tile_color_index(val);

        write_str(tile_bg_colors[ci]);
        write_str(tile_fg_colors[ci]);

        if (val == 0) {
            int sp = CELL_W;
            char space = ' ';
            while (sp > 0) { sys_write(1, &space, 1); sp--; }
        } else {
            int nlen = 0;
            if (val == 0) nlen = 1;
            else {
                int t = val;
                while (t > 0) { nlen++; t /= 10; }
            }

            int pad = CELL_W - nlen;
            char space = ' ';
            while (pad > 0) { sys_write(1, &space, 1); pad--; }

            int nb = 0;
            int digits[12];
            int ndigits = 0;
            int t = val;
            while (t > 0) { digits[ndigits++] = t % 10; t /= 10; }
            if (ndigits == 0) digits[ndigits++] = 0;
            for (int k = ndigits - 1; k >= 0; k--) numbuf[nb++] = '0' + digits[k];
            numbuf[nb] = '\0';
            sys_write(1, numbuf, nb);
        }

        reset_colors();
        write_str("|");
    }

    /* --- Row 2: blank (extends cell height for square appearance) --- */
    cursor_to(GRID_START_X, y + 1);
    write_str("|");
    for (c = 0; c < GRID_COLS; c++) {
        write_str(tile_bg_colors[tile_color_index(board[row_idx][c])]);
        {
            int sp = CELL_W;
            char space = ' ';
            while (sp > 0) { sys_write(1, &space, 1); sp--; }
        }
        reset_colors();
        write_str("|");
    }
}

static void render_board(void)
{
    int r;
    cursor_to(GRID_START_X, GRID_TOP_Y);
    draw_grid_top_border();
    for (r = 0; r < GRID_ROWS; r++) {
        draw_cell_content(r);
        /* Border after each cell (2 rows of content + 1 border row) */
        cursor_to(GRID_START_X, GRID_TOP_Y + 1 + r * 3 + 2);
        draw_grid_top_border();
    }
}

static void render_scores(void)
{
    char buf[32];
    int n;

    /* Score + Best on one line */
    cursor_to(GRID_START_X, SCORE_Y);
    write_str("\x1b[K");
    cursor_to(GRID_START_X, SCORE_Y);
    write_str("Score: ");
    n = 0;
    n = append_uint(buf, n, score);
    sys_write(1, buf, n);
    write_str("   Best: ");
    n = 0;
    n = append_uint(buf, n, best_score);
    sys_write(1, buf, n);
}

static void render_hints(void)
{
    cursor_to(GRID_START_X, HINT1_Y);
    write_str("\x1b[K");
    cursor_to(GRID_START_X, HINT1_Y);
    write_str("WASD/Arrows: move");

    cursor_to(GRID_START_X, HINT2_Y);
    write_str("\x1b[K");
    cursor_to(GRID_START_X, HINT2_Y);
    write_str("R: new game");

    cursor_to(GRID_START_X, HINT3_Y);
    write_str("\x1b[K");
    cursor_to(GRID_START_X, HINT3_Y);
    write_str("Q/Esc: quit");
}

static void render_message(const char *msg)
{
    int i, mlen = 0;
    while (msg[mlen]) mlen++;
    int start = (40 - mlen) / 2;
    if (start < 0) start = 0;
    cursor_to(0, MSG_Y);
    write_str("\x1b[K");
    cursor_to(start, MSG_Y);
    write_str("\x1b[1m");
    write_str(msg);
    write_str("\x1b[0m");
}

static void clear_message(void)
{
    cursor_to(0, MSG_Y);
    write_str("\x1b[K");
}

static void full_render(void)
{
    draw_title();
    render_board();
    render_scores();
    render_hints();
}

/* ---- Game init ---- */

static void init_game(void)
{
    clear_board();
    score = 0;
    won_game = 0;
    game_over = 0;
    place_random_tile();
    place_random_tile();
    clear_message();
    full_render();
}

/* ---- Input handling ---- */

static int process_input(void)
{
    int key;

    while ((key = sys_tty_event_read(tty_fd, 0)) >= 0) {
        int dir = -1;

        if (key == 27 || key == 'q' || key == 'Q') return 1;

        if (key == 'r' || key == 'R') {
            init_game();
            return 0;
        }

        if (key == KEY_LEFT || key == 'a' || key == 'A') dir = DIR_LEFT;
        else if (key == KEY_RIGHT || key == 'd' || key == 'D') dir = DIR_RIGHT;
        else if (key == KEY_UP || key == 'w' || key == 'W') dir = DIR_UP;
        else if (key == KEY_DOWN || key == 's' || key == 'S') dir = DIR_DOWN;

        if (dir >= 0 && !game_over) {
            if (slide_board(dir)) {
                place_random_tile();

                if (score > best_score) best_score = score;

                if (!won_game && check_win()) {
                    won_game = 1;
                    render_scores();
                    render_message("YOU WIN! Keep going or R to restart");
                    render_board();
                } else if (!can_move()) {
                    game_over = 1;
                    render_scores();
                    render_message("GAME OVER! R to restart, Q to quit");
                    render_board();
                } else {
                    clear_message();
                    render_scores();
                    render_board();
                }
            }
        }
    }
    return 0;
}

/* ---- Main ---- */

int main(void)
{
    tty_fd = sys_tty_open_raw(1 /* nonblocking */);
    if (tty_fd < 0) {
        write_str("2048: cannot open /dev/tty in raw mode\n");
        return 1;
    }

    /* Seed RNG */
    rng_lfsr = (unsigned int)sys_get_time_us();
    if (rng_lfsr == 0) rng_lfsr = 0xACE1u;

    best_score = 0;

    /* Enter alternate screen */
    write_str("\x1b[?1049h");
    clear_screen();

    init_game();

    while (1) {
        if (process_input()) break;
        sys_sleep(TICK_DELAY_MS);
    }

    /* Leave alternate screen, restore previous view */
    write_str("\x1b[?1049l");
    sys_close(tty_fd);
    return 0;
}
