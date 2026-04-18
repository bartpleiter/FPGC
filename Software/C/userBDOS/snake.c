/*
 * snake.c — Snake game, ported to libterm v2 / VFS API.
 *
 * Phase E reference port. Uses only the post-Phase-E user API:
 *   - sys_write(1, ...) with ANSI escapes for screen output
 *   - /dev/tty opened in raw + non-blocking mode for key events
 *   - sys_get_key_state() for held-key polling (kept for parity with
 *     the rest of the userBDOS games)
 *
 * Controls: arrow keys / WASD to move, +/= to speed up, Esc / Q to quit.
 */

#include <syscall.h>

/* ---- Timing ---- */
#define START_DELAY_MS 120
#define STEP_DELAY_MS  20
#define MIN_DELAY_MS   30
#define TICK_DELAY_MS  10

/* ---- Board (40x25 screen, last row = score bar) ---- */
#define BOARD_WIDTH  40
#define BOARD_HEIGHT 24
#define SCORE_Y      24

/* ---- Directions ---- */
#define DIR_UP    0
#define DIR_DOWN  1
#define DIR_RIGHT 2
#define DIR_LEFT  3

/* ---- Glyphs (printable ASCII so terminal emit doesn't swallow them) ---- */
#define CHAR_WALL  '#'
#define CHAR_BODY  'o'
#define CHAR_FOOD  '*'
#define CHAR_UP    '^'
#define CHAR_DOWN  'v'
#define CHAR_RIGHT '>'
#define CHAR_LEFT  '<'

#define MAX_SNAKE_LEN ((BOARD_WIDTH - 2) * (BOARD_HEIGHT - 2))

int snake_x[900];
int snake_y[900];
int snake_len;

int dir;
int dir_on_screen;

int score;
int delay_time;

int food_x;
int food_y;

int game_over;

int tty_fd; /* /dev/tty O_RAW|O_NONBLOCK fd */

unsigned int rng_lfsr = 0xACE1;

int rng_rand(void)
{
  int bit;
  bit = ((rng_lfsr >> 0) ^ (rng_lfsr >> 2) ^ (rng_lfsr >> 3) ^ (rng_lfsr >> 5)) & 1;
  rng_lfsr = (rng_lfsr >> 1) | (bit << 15);
  return rng_lfsr;
}

int rng_mod(int val, int modulus)
{
  if (val < 0) val = -val;
  return val % modulus;
}

/* ---- Tiny output helpers (no libc printf available) ---- */

static int write_str(const char *s)
{
  int n = 0;
  while (s[n]) n++;
  return sys_write(1, s, n);
}

/* Append decimal representation of `val` (>=0) to `out`, returning new length. */
static int append_uint(char *out, int len, int val)
{
  char tmp[12];
  int  i = 0;
  if (val == 0) { tmp[i++] = '0'; }
  else {
    while (val > 0) { tmp[i++] = (char)('0' + (val % 10)); val /= 10; }
  }
  while (i > 0) { out[len++] = tmp[--i]; }
  return len;
}

/* Move cursor to (x, y) — 0-based, converted to 1-based for ANSI CUP. */
static void cursor_to(int x, int y)
{
  char buf[16];
  int  n = 0;
  buf[n++] = '\x1b';
  buf[n++] = '[';
  n = append_uint(buf, n, y + 1);
  buf[n++] = ';';
  n = append_uint(buf, n, x + 1);
  buf[n++] = 'H';
  sys_write(1, buf, n);
}

static void put_tile(int x, int y, int ch)
{
  char c = (char)ch;
  cursor_to(x, y);
  sys_write(1, &c, 1);
}

static void clear_screen(void)
{
  /* CSI 2 J  +  CSI H */
  write_str("\x1b[2J\x1b[H");
}

/* ---- Rendering ---- */

void draw_border(void)
{
  int x;
  int y;
  for (x = 0; x < BOARD_WIDTH; x++) {
    put_tile(x, 0, CHAR_WALL);
    put_tile(x, BOARD_HEIGHT - 1, CHAR_WALL);
  }
  for (y = 0; y < BOARD_HEIGHT; y++) {
    put_tile(0, y, CHAR_WALL);
    put_tile(BOARD_WIDTH - 1, y, CHAR_WALL);
  }
}

void draw_score(void)
{
  char buf[64];
  int  n;
  int  i;

  /* Move to score row, clear to end of line, then print "Score: N". */
  cursor_to(0, SCORE_Y);
  write_str("\x1b[K");          /* erase from cursor to end of line */

  n = 0;
  buf[n++] = 'S'; buf[n++] = 'c'; buf[n++] = 'o'; buf[n++] = 'r'; buf[n++] = 'e';
  buf[n++] = ':'; buf[n++] = ' ';
  n = append_uint(buf, n, score);
  for (i = 0; i < n; i++) sys_write(1, &buf[i], 1);
}

void draw_food(void)
{
  put_tile(food_x, food_y, CHAR_FOOD);
}

void draw_snake(void)
{
  int head_char;
  int i;
  if      (dir == DIR_UP)    head_char = CHAR_UP;
  else if (dir == DIR_DOWN)  head_char = CHAR_DOWN;
  else if (dir == DIR_LEFT)  head_char = CHAR_LEFT;
  else                       head_char = CHAR_RIGHT;
  put_tile(snake_x[0], snake_y[0], head_char);
  for (i = 1; i < snake_len; i++)
    put_tile(snake_x[i], snake_y[i], CHAR_BODY);
}

/* ---- Food generation ---- */

int gen_food_x(void) { return rng_mod(rng_rand(), BOARD_WIDTH  - 2) + 1; }
int gen_food_y(void) { return rng_mod(rng_rand(), BOARD_HEIGHT - 2) + 1; }

int collides_with_snake(int x, int y)
{
  int i;
  for (i = 0; i < snake_len; i++)
    if (x == snake_x[i] && y == snake_y[i]) return 1;
  return 0;
}

void place_food(void)
{
  food_x = gen_food_x();
  food_y = gen_food_y();
  while (collides_with_snake(food_x, food_y)) {
    food_x = gen_food_x();
    food_y = gen_food_y();
  }
}

/* ---- Game logic ---- */

int check_food_collision(void)
{
  if (snake_x[0] == food_x && snake_y[0] == food_y) {
    score++;
    snake_len++;
    place_food();
    return 1;
  }
  return 0;
}

int check_game_over(void)
{
  int i;
  if (snake_x[0] == 0 || snake_x[0] == BOARD_WIDTH - 1 ||
      snake_y[0] == 0 || snake_y[0] == BOARD_HEIGHT - 1) return 1;
  for (i = 1; i < snake_len; i++)
    if (snake_x[0] == snake_x[i] && snake_y[0] == snake_y[i]) return 1;
  return 0;
}

void update_snake(int keep_tail)
{
  int i;
  if (!keep_tail)
    put_tile(snake_x[snake_len - 1], snake_y[snake_len - 1], ' ');
  for (i = snake_len - 2; i >= 0; i--) {
    snake_x[i + 1] = snake_x[i];
    snake_y[i + 1] = snake_y[i];
  }
  if      (dir == DIR_UP)   { snake_y[0] = snake_y[1] - 1; snake_x[0] = snake_x[1]; }
  else if (dir == DIR_DOWN) { snake_y[0] = snake_y[1] + 1; snake_x[0] = snake_x[1]; }
  else if (dir == DIR_LEFT) { snake_x[0] = snake_x[1] - 1; snake_y[0] = snake_y[1]; }
  else                      { snake_x[0] = snake_x[1] + 1; snake_y[0] = snake_y[1]; }
  dir_on_screen = dir;
}

void do_game_over(void)
{
  const char *msg = "GAME OVER!";
  int x = (BOARD_WIDTH / 2) - 5;
  int i = 0;
  while (msg[i]) { put_tile(x + i, BOARD_HEIGHT / 2, msg[i]); i++; }
  game_over = 1;
}

void game_tick(void)
{
  int keep_tail = check_food_collision();
  update_snake(keep_tail);
  draw_score();
  draw_food();
  draw_snake();
  if (check_game_over()) do_game_over();
}

/* ---- Input handling ----
 *
 * Reads zero or more 4-byte event packets from the raw /dev/tty fd. Each
 * packet is a key code: ASCII codes for printable keys, KEY_UP / KEY_F1
 * / etc. for special keys, and 27 for Escape.
 */
int process_input(void)
{
  int key;
  while ((key = sys_tty_event_read(tty_fd, 0)) >= 0) {
    if (key == 27 || key == 'q' || key == 'Q') return 1;
    if (key == KEY_LEFT  || key == 'a' || key == 'A') {
      if (dir_on_screen != DIR_RIGHT) dir = DIR_LEFT;
    } else if (key == KEY_RIGHT || key == 'd' || key == 'D') {
      if (dir_on_screen != DIR_LEFT)  dir = DIR_RIGHT;
    } else if (key == KEY_UP    || key == 'w' || key == 'W') {
      if (dir_on_screen != DIR_DOWN)  dir = DIR_UP;
    } else if (key == KEY_DOWN  || key == 's' || key == 'S') {
      if (dir_on_screen != DIR_UP)    dir = DIR_DOWN;
    } else if (key == '=' || key == '+') {
      if (delay_time > MIN_DELAY_MS + STEP_DELAY_MS)
        delay_time -= STEP_DELAY_MS;
    }
  }
  return 0;
}

/* ---- Initialization ---- */

void init_game(void)
{
  snake_len = 3;
  dir = DIR_RIGHT;
  dir_on_screen = DIR_RIGHT;
  score = 0;
  delay_time = START_DELAY_MS;
  game_over = 0;

  snake_x[0] = (BOARD_WIDTH / 4) - 1;
  snake_y[0] = BOARD_HEIGHT / 2;
  snake_x[1] = snake_x[0] - 1; snake_y[1] = snake_y[0];
  snake_x[2] = snake_x[1] - 1; snake_y[2] = snake_y[1];

  food_x = (BOARD_WIDTH / 2) + (BOARD_WIDTH / 4);
  food_y = (BOARD_HEIGHT / 2) - (BOARD_HEIGHT / 4);

  /* Enter alternate screen so the prior shell view is restored on exit. */
  write_str("\x1b[?1049h");
  clear_screen();
  draw_border();
  draw_score();
  draw_food();
  draw_snake();
}

/* ---- Main ---- */

int main(void)
{
  int elapsed = 0;

  tty_fd = sys_tty_open_raw(1 /* nonblocking */);
  if (tty_fd < 0) {
    write_str("snake: cannot open /dev/tty in raw mode\n");
    return 1;
  }

  init_game();

  while (1) {
    if (process_input()) break;
    if (!game_over) {
      elapsed += TICK_DELAY_MS;
      if (elapsed >= delay_time) {
        game_tick();
        elapsed = 0;
      }
    }
    rng_rand();
    sys_delay(TICK_DELAY_MS);
  }

  /* Leave alternate screen, restoring previous view; close TTY fd. */
  write_str("\x1b[?1049l");
  sys_close(tty_fd);
  return 0;
}
