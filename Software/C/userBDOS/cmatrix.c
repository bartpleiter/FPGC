/*
 * FIXME: shell-terminal-v2 Phase E migration TODO.
 *
 * This program still uses syscalls that were removed from BDOS in
 * Phase E. It will currently fail to link or behave correctly. See
 * the migration table at the top of Software/C/userlib/include/syscall.h.
 *
 * Quick checklist for porting:
 *   sys_term_put_cell / sys_term_clear / sys_term_set_cursor
 *      -> sys_write(1, "\x1b[<y+1>;<x+1>H...", n) ANSI escapes.
 *      -> Glyphs that overlap C0 control codes (BEL, HT, LF, ESC, ...)
 *         must be substituted with printable ASCII.
 *   sys_read_key / sys_key_available
 *      -> int fd = sys_tty_open_raw(1);            (non-blocking)
 *         int ev = sys_tty_event_read(fd, 0);
 *      -> snake.c is the reference port.
 *   sys_set_palette / sys_set_pixel_palette
 *      -> No replacement syscall yet. Either use ANSI SGR colors
 *         (\x1b[30m..37m for tile palette 0..7) or wait for a
 *         dedicated palette syscall to be added in a follow-up.
 *   sys_uart_print_str / sys_uart_print_char
 *      -> sys_write(2, s, n) (stderr; mirrored to UART by libterm).
 */

// cmatrix.c — Matrix rain effect for FPGC BDOS
// Press Escape or Q to exit.

#include <syscall.h>

#define SCREEN_WIDTH  40
#define SCREEN_HEIGHT 25

#define MAX_RAIN_LENGTH 15
#define MIN_RAIN_LENGTH 5
#define RAIN_DELAY_MS   32  // ~30fps

// Palette indices
#define PAL_GREEN 0  // We'll set palette 0 to green-on-black at startup

// LFSR-based RNG
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

// Rain status array: tracks remaining rain length at each cell.
// 0 = empty, >0 = rain drops remaining to propagate downward.
// Stored as a flat array [y * SCREEN_WIDTH + x].
int *rain_status;

// Helper: put a character on screen with green palette
void put_ch(int x, int y, int ch)
{
  sys_term_put_cell(x, y, (ch << 8) | PAL_GREEN);
}

// Return a random printable ASCII character (33..126)
int random_char(void)
{
  rng_rand();
  rng_rand();
  return rng_mod(rng_rand(), 94) + 33;
}

// Update all rain on screen — propagate downward
void update_rain(void)
{
  int x;
  int y;
  int idx;
  int idx_below;

  // Process last row: decrement, clear when zero
  y = SCREEN_HEIGHT - 1;
  for (x = 0; x < SCREEN_WIDTH; x++)
  {
    idx = y * SCREEN_WIDTH + x;
    if (rain_status[idx] > 0)
    {
      rain_status[idx]--;
      if (rain_status[idx] == 0)
      {
        put_ch(x, y, 0);
      }
    }
  }

  // Process rows bottom-up (skip last row)
  for (y = SCREEN_HEIGHT - 2; y >= 0; y--)
  {
    for (x = 0; x < SCREEN_WIDTH; x++)
    {
      idx = y * SCREEN_WIDTH + x;
      if (rain_status[idx] > 0)
      {
        idx_below = (y + 1) * SCREEN_WIDTH + x;

        // Create new char below if empty
        if (rain_status[idx_below] == 0)
        {
          put_ch(x, y + 1, random_char());
        }

        // Propagate status downward
        rain_status[idx_below] = rain_status[idx];

        // Decrement current cell
        rain_status[idx]--;
        if (rain_status[idx] == 0)
        {
          put_ch(x, y, 0);
        }
      }
    }
  }
}

// Generate a new rain column at position x
void gen_rain_line(int x)
{
  int idx;
  int idx1;
  int rain_len;

  // Don't start if top cells aren't free
  idx = x;       // y=0
  idx1 = SCREEN_WIDTH + x; // y=1
  if (rain_status[idx] != 0 || rain_status[idx1] != 0)
  {
    return;
  }

  rain_len = rng_mod(rng_rand(), (MAX_RAIN_LENGTH + 1) - MIN_RAIN_LENGTH) + MIN_RAIN_LENGTH;
  rain_status[idx] = rain_len;
  put_ch(x, 0, random_char());
}

int main(void)
{
  int key;
  int i;

  // Allocate rain status array on heap
  rain_status = (int *)sys_heap_alloc(SCREEN_WIDTH * SCREEN_HEIGHT);
  if (rain_status == (int *)0)
  {
    sys_putstr("cmatrix: out of memory\n");
    return 1;
  }

  // Zero out rain status
  for (i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++)
  {
    rain_status[i] = 0;
  }

  // Set palette 0 to green-on-black
  // Color format: (bg_color << 8) | fg_color
  // 8-bit color: RRRGGGBB
  // Green = 0b00011100 = 28, Black = 0
  sys_set_palette(0, (0 << 8) | 28);

  /* Enter alternate screen so the prior shell view is restored on exit. */
  sys_putstr("\033[?1049h");
  sys_term_clear();

  // Main loop
  while (1)
  {
    // Check for exit key
    if (sys_key_available())
    {
      key = sys_read_key();
      if (key == 27 || key == 'q' || key == 'Q')
      {
        break;
      }
    }

    update_rain();

    // Advance RNG and spawn new rain columns
    rng_rand();
    rng_rand();
    rng_rand();
    gen_rain_line(rng_mod(rng_rand(), SCREEN_WIDTH));
    rng_rand();
    rng_rand();
    rng_rand();
    gen_rain_line(rng_mod(rng_rand(), SCREEN_WIDTH));

    sys_delay(RAIN_DELAY_MS);
  }

  // Restore default palette (white-on-black). Leaving the alt screen
  // restores whatever was visible before cmatrix ran.
  sys_set_palette(0, (0 << 8) | 0xFF);
  sys_putstr("\033[?1049l");

  return 0;
}
