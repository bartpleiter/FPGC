// cmatrix.c — Matrix rain effect for FPGC BDOS
//
// Uses ANSI escape sequences on fd 1 for output and the raw /dev/tty
// event stream (sys_tty_open_raw / sys_tty_event_read) for input. The
// foreground green colour is selected with SGR \x1b[32m once at
// startup and reset with \x1b[0m on exit.
//
// Press Escape or Q to exit.

#include <syscall.h>

#define SCREEN_WIDTH  40
#define SCREEN_HEIGHT 25

#define MAX_RAIN_LENGTH 15
#define MIN_RAIN_LENGTH 5
#define RAIN_DELAY_MS   32  // ~30fps

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

// Raw /dev/tty fd for non-blocking key event reads.
int tty_fd;

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

// Helper: put a character at (x, y). ch == 0 erases the cell.
void put_ch(int x, int y, int ch)
{
  char c;
  cursor_to(x, y);
  c = (ch == 0) ? ' ' : (char)ch;
  sys_write(1, &c, 1);
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

/* Drain pending key events, return non-zero to request exit. */
static int process_input(void)
{
  int key;
  while ((key = sys_tty_event_read(tty_fd, 0)) >= 0)
  {
    if (key == 27 || key == 'q' || key == 'Q')
    {
      return 1;
    }
  }
  return 0;
}

int main(void)
{
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

  tty_fd = sys_tty_open_raw(1 /* nonblocking */);
  if (tty_fd < 0)
  {
    sys_putstr("cmatrix: cannot open /dev/tty in raw mode\n");
    return 1;
  }

  /* Enter alternate screen, disable auto-wrap (so writing the
   * bottom-right cell doesn't scroll), set foreground green,
   * clear screen. Restored to the previous shell view on exit. */
  write_str("\x1b[?1049h\x1b[?7l\x1b[32m\x1b[2J\x1b[H");

  // Main loop
  while (1)
  {
    if (process_input())
    {
      break;
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

  /* Reset SGR, re-enable auto-wrap, leave alternate screen. */
  write_str("\x1b[0m\x1b[?7h\x1b[?1049l");
  sys_close(tty_fd);

  return 0;
}
