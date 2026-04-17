// snake.c — Snake game for FPGC BDOS
// Arrow keys or WASD to move, +/= to speed up, Escape/Q to quit.

#include <syscall.h>

// Timing
#define START_DELAY_MS 120
#define STEP_DELAY_MS  20
#define MIN_DELAY_MS   30
#define TICK_DELAY_MS  10  // Poll interval within each game tick

// Board dimensions (40x25 screen, row 24 = score bar)
#define BOARD_WIDTH  40
#define BOARD_HEIGHT 24
#define SCORE_Y      24

// Directions
#define DIR_UP    0
#define DIR_DOWN  1
#define DIR_RIGHT 2
#define DIR_LEFT  3

// Tile characters (from the default ASCII pattern table)
#define CHAR_WALL  219
#define CHAR_BODY  177
#define CHAR_FOOD  7
#define CHAR_UP    30
#define CHAR_DOWN  31
#define CHAR_RIGHT 16
#define CHAR_LEFT  17

// Palette indices
#define PAL_DEFAULT 0

// Max snake length
#define MAX_SNAKE_LEN ((BOARD_WIDTH - 2) * (BOARD_HEIGHT - 2))

// Snake body coordinates (stored as parallel arrays)
int snake_x[900];  // MAX_SNAKE_LEN = 38*22 = 836, round up
int snake_y[900];
int snake_len;

int dir;            // Current direction
int dir_on_screen;  // Direction of last rendered head (prevents 180-degree turn)

int score;
int delay_time;

int food_x;
int food_y;

int game_over;

// LFSR RNG
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
  if (val < 0)
  {
    val = -val;
  }
  return val % modulus;
}

// ---- Rendering helpers ----

void put_tile(int x, int y, int ch)
{
  sys_term_put_cell(x, y, (ch << 8) | PAL_DEFAULT);
}

void draw_border(void)
{
  int x;
  int y;

  // Top and bottom
  for (x = 0; x < BOARD_WIDTH; x++)
  {
    put_tile(x, 0, CHAR_WALL);
    put_tile(x, BOARD_HEIGHT - 1, CHAR_WALL);
  }

  // Left and right
  for (y = 0; y < BOARD_HEIGHT; y++)
  {
    put_tile(0, y, CHAR_WALL);
    put_tile(BOARD_WIDTH - 1, y, CHAR_WALL);
  }
}

void int_to_str(int val, char *out)
{
  char tmp[12];
  int i;
  int j;

  i = 0;
  if (val == 0)
  {
    tmp[i] = '0';
    i++;
  }
  else
  {
    while (val > 0)
    {
      tmp[i] = '0' + (val % 10);
      val = val / 10;
      i++;
    }
  }

  // Reverse
  j = 0;
  while (i > 0)
  {
    i--;
    out[j] = tmp[i];
    j++;
  }
  out[j] = 0;
}

void draw_score(void)
{
  char buf[12];
  int i;
  char *label = "Score:";

  // Clear score row
  for (i = 0; i < BOARD_WIDTH; i++)
  {
    put_tile(i, SCORE_Y, ' ');
  }

  // Draw label
  i = 0;
  while (label[i] != 0)
  {
    put_tile(i, SCORE_Y, label[i]);
    i++;
  }

  // Draw score number
  int_to_str(score, buf);
  i = 0;
  while (buf[i] != 0)
  {
    put_tile(7 + i, SCORE_Y, buf[i]);
    i++;
  }
}

void draw_food(void)
{
  put_tile(food_x, food_y, CHAR_FOOD);
}

void draw_snake(void)
{
  int head_char;
  int i;

  // Draw head with direction arrow
  if (dir == DIR_UP)
  {
    head_char = CHAR_UP;
  }
  else if (dir == DIR_DOWN)
  {
    head_char = CHAR_DOWN;
  }
  else if (dir == DIR_LEFT)
  {
    head_char = CHAR_LEFT;
  }
  else
  {
    head_char = CHAR_RIGHT;
  }
  put_tile(snake_x[0], snake_y[0], head_char);

  // Draw body
  for (i = 1; i < snake_len; i++)
  {
    put_tile(snake_x[i], snake_y[i], CHAR_BODY);
  }
}

// ---- Food generation ----

int gen_food_x(void)
{
  int x;
  x = rng_mod(rng_rand(), BOARD_WIDTH - 2) + 1;
  return x;
}

int gen_food_y(void)
{
  int y;
  y = rng_mod(rng_rand(), BOARD_HEIGHT - 2) + 1;
  return y;
}

int collides_with_snake(int x, int y)
{
  int i;
  for (i = 0; i < snake_len; i++)
  {
    if (x == snake_x[i] && y == snake_y[i])
    {
      return 1;
    }
  }
  return 0;
}

void place_food(void)
{
  food_x = gen_food_x();
  food_y = gen_food_y();
  while (collides_with_snake(food_x, food_y))
  {
    food_x = gen_food_x();
    food_y = gen_food_y();
  }
}

// ---- Game logic ----

// Returns 1 if food was eaten
int check_food_collision(void)
{
  if (snake_x[0] == food_x && snake_y[0] == food_y)
  {
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

  // Collision with border
  if (snake_x[0] == 0 || snake_x[0] == BOARD_WIDTH - 1 ||
      snake_y[0] == 0 || snake_y[0] == BOARD_HEIGHT - 1)
  {
    return 1;
  }

  // Collision with self
  for (i = 1; i < snake_len; i++)
  {
    if (snake_x[0] == snake_x[i] && snake_y[0] == snake_y[i])
    {
      return 1;
    }
  }

  return 0;
}

void update_snake(int keep_tail)
{
  int i;

  // Erase tail if not keeping it (didn't eat food)
  if (!keep_tail)
  {
    put_tile(snake_x[snake_len - 1], snake_y[snake_len - 1], 0);
  }

  // Shift body segments backward
  for (i = snake_len - 2; i >= 0; i--)
  {
    snake_x[i + 1] = snake_x[i];
    snake_y[i + 1] = snake_y[i];
  }

  // Move head in current direction
  if (dir == DIR_UP)
  {
    snake_y[0] = snake_y[1] - 1;
    snake_x[0] = snake_x[1];
  }
  else if (dir == DIR_DOWN)
  {
    snake_y[0] = snake_y[1] + 1;
    snake_x[0] = snake_x[1];
  }
  else if (dir == DIR_LEFT)
  {
    snake_x[0] = snake_x[1] - 1;
    snake_y[0] = snake_y[1];
  }
  else
  {
    snake_x[0] = snake_x[1] + 1;
    snake_y[0] = snake_y[1];
  }

  dir_on_screen = dir;
}

void do_game_over(void)
{
  char *msg = "GAME OVER!";
  int x;
  int i;

  x = (BOARD_WIDTH / 2) - 5;
  i = 0;
  while (msg[i] != 0)
  {
    put_tile(x + i, BOARD_HEIGHT / 2, msg[i]);
    i++;
  }
  game_over = 1;
}

void game_tick(void)
{
  int keep_tail;

  keep_tail = check_food_collision();
  update_snake(keep_tail);
  draw_score();
  draw_food();
  draw_snake();
  if (check_game_over())
  {
    do_game_over();
  }
}

// ---- Input handling (event-based) ----

// Drain all pending keys, updating direction from the last arrow key seen.
// Returns 1 if user wants to quit, 0 otherwise.
int process_input(void)
{
  int key;

  while (sys_key_available())
  {
    key = sys_read_key();

    if (key == 27 || key == 'q' || key == 'Q')
    {
      return 1;
    }

    // Arrow keys
    if (key == KEY_LEFT || key == 'a' || key == 'A')
    {
      if (dir_on_screen != DIR_RIGHT)
      {
        dir = DIR_LEFT;
      }
    }
    else if (key == KEY_RIGHT || key == 'd' || key == 'D')
    {
      if (dir_on_screen != DIR_LEFT)
      {
        dir = DIR_RIGHT;
      }
    }
    else if (key == KEY_UP || key == 'w' || key == 'W')
    {
      if (dir_on_screen != DIR_DOWN)
      {
        dir = DIR_UP;
      }
    }
    else if (key == KEY_DOWN || key == 's' || key == 'S')
    {
      if (dir_on_screen != DIR_UP)
      {
        dir = DIR_DOWN;
      }
    }
    else if (key == '=' || key == '+')
    {
      if (delay_time > MIN_DELAY_MS + STEP_DELAY_MS)
      {
        delay_time = delay_time - STEP_DELAY_MS;
      }
    }
  }

  return 0;
}

// ---- Initialization ----

void init_game(void)
{
  snake_len = 3;
  dir = DIR_RIGHT;
  dir_on_screen = DIR_RIGHT;
  score = 0;
  delay_time = START_DELAY_MS;
  game_over = 0;

  // Starting position: left quarter, middle
  snake_x[0] = (BOARD_WIDTH / 4) - 1;
  snake_y[0] = BOARD_HEIGHT / 2;
  snake_x[1] = snake_x[0] - 1;
  snake_y[1] = snake_y[0];
  snake_x[2] = snake_x[1] - 1;
  snake_y[2] = snake_y[1];

  // Initial food position: right quarter, upper quarter
  food_x = (BOARD_WIDTH / 2) + (BOARD_WIDTH / 4);
  food_y = (BOARD_HEIGHT / 2) - (BOARD_HEIGHT / 4);

  /* Enter alternate screen so the prior shell view is restored on exit. */
  sys_print_str("\033[?1049h");
  sys_term_clear();
  draw_border();
  draw_score();
  draw_food();
  draw_snake();
}

// ---- Main ----

int main(void)
{
  int elapsed;
  int quit;

  init_game();

  // Main loop: poll input at a fast rate, tick game at delay_time intervals
  elapsed = 0;

  while (1)
  {
    quit = process_input();
    if (quit)
    {
      break;
    }

    if (!game_over)
    {
      elapsed = elapsed + TICK_DELAY_MS;
      if (elapsed >= delay_time)
      {
        game_tick();
        elapsed = 0;
      }
    }

    // Advance RNG
    rng_rand();

    sys_delay(TICK_DELAY_MS);
  }

  // Cleanup — leave alternate screen, restoring the previous view.
  sys_print_str("\033[?1049l");
  return 0;
}
