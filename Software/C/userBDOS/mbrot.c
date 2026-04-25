/*
 * mbrot.c — Standalone Mandelbrot zoom animation (userBDOS).
 */

//
// mbrot.c — Standalone Mandelbrot zoom animation (userBDOS).
// Renders directly on a single device using the FP64 coprocessor
// via assembly helpers (mbrot_asm.asm) for maximum performance.
//

#include <syscall.h>
#include <plot.h>

// ---- ANSI / VFS shims for retired syscalls ----
// /dev/pixpal is the 8-bit pixel-palette DAC; /dev/tty raw stream
// supplies non-blocking key events. Both fds are opened in main().

static int g_pixpal_fd = -1;
static int g_tty_fd    = -1;

static void term_clear(void)
{
  sys_write(1, "\x1b[2J\x1b[H", 7);
}

static void pixpal_load_all(const int *entries)
{
  if (g_pixpal_fd < 0) return;
  sys_lseek(g_pixpal_fd, 0, SEEK_SET);
  sys_write(g_pixpal_fd, entries, 256 * 4);
}

// ---- Screen constants ----
#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240

// ---- Off-screen buffer ----
char *backbuf;

// Upscale source rect for 0.90x zoom (center 90% of image)
#define UPSCALE_X0  16
#define UPSCALE_Y0  12
#define UPSCALE_W   288
#define UPSCALE_H   216

// ---- Palette data ----
#define NUM_PALETTES 5
int current_palette;
int palette[256];

// ---- View state (Q32.32 fixed-point, stored as hi/lo pairs) ----
int center_re_hi;
unsigned int center_re_lo;
int center_im_hi;
unsigned int center_im_lo;
int scale_hi;
unsigned int scale_lo;
int max_iter;

// Minimum scale before reset (~1e-7 in Q32.32)
#define MIN_SCALE_LO 0x00001000

// Zoom factor per frame: 0.90 in Q32.32
#define ZOOM_FACTOR_LO 0xE6666666

// ---- Auto-zoom state ----
int auto_zoom;

// ---- Assembly FP64 helpers (mbrot_asm.asm) ----
extern void mbrot_load_cre(int hi, int lo);
extern void mbrot_load_cim(int hi, int lo);
extern void mbrot_load_step(int hi, int lo);
extern void mbrot_advance_cre(void);
extern void mbrot_advance_cim(void);
extern int  mbrot_store_hi_cre(void);
extern int  mbrot_store_lo_cre(void);
extern int  mbrot_store_hi_step(void);
extern int  mbrot_store_lo_step(void);
extern void mbrot_load_f6(int hi, int lo);
extern void mbrot_load_f7(int hi, int lo);
extern void mbrot_mul_f7_f6(void);
extern void mbrot_sub_f0_f6(void);
extern void mbrot_sub_f1_f6(void);
extern int  mbrot_store_hi_f7(void);
extern int  mbrot_store_lo_f7(void);
extern int  mbrot_mandelbrot_pixel(int max_iter);

// ---- Palette generation ----

int lerp_color(int c0, int c1, int t)
{
  int r0;
  int g0;
  int b0;
  int r1;
  int g1;
  int b1;
  int r;
  int g;
  int b;

  r0 = (c0 >> 16) & 0xFF;
  g0 = (c0 >> 8) & 0xFF;
  b0 = c0 & 0xFF;
  r1 = (c1 >> 16) & 0xFF;
  g1 = (c1 >> 8) & 0xFF;
  b1 = c1 & 0xFF;

  r = r0 + ((r1 - r0) * t) / 255;
  g = g0 + ((g1 - g0) * t) / 255;
  b = b0 + ((b1 - b0) * t) / 255;

  return (r << 16) | (g << 8) | b;
}

void build_gradient(int *stops_pos, int *stops_color, int num_stops)
{
  int i;
  int seg;

  palette[0] = 0x000000;

  for (i = 1; i < 256; i++)
  {
    seg = 0;
    while (seg < num_stops - 2 && i > stops_pos[seg + 1])
    {
      seg = seg + 1;
    }

    {
      int seg_start = stops_pos[seg];
      int seg_end = stops_pos[seg + 1];
      int seg_len = seg_end - seg_start;
      int t;
      if (seg_len > 0)
      {
        t = ((i - seg_start) * 255) / seg_len;
      }
      else
      {
        t = 0;
      }
      palette[i] = lerp_color(stops_color[seg], stops_color[seg + 1], t);
    }
  }
}

void load_palette_classic(void)
{
  int pos[6]   = {1,   51,  102,  153,  204,  255};
  int col[6] = {0x000080, 0x00FFFF, 0x00FF00, 0xFFFF00, 0xFF0000, 0x000000};
  build_gradient(pos, col, 6);
}

void load_palette_fire(void)
{
  int pos[5]   = {1,   64,  128,  192,  255};
  int col[5] = {0x200000, 0xAA0000, 0xFF6600, 0xFFFF00, 0xFFFFFF};
  build_gradient(pos, col, 5);
}

void load_palette_ice(void)
{
  int pos[4]   = {1,   85,  170,  255};
  int col[4] = {0x000020, 0x0000CC, 0x00CCFF, 0xFFFFFF};
  build_gradient(pos, col, 4);
}

void load_palette_ultra(void)
{
  int pos[7]   = {1,   42,  85,  128,  170,  213,  255};
  int col[7] = {0x000764, 0x206BCB, 0xEDFFFF, 0xFFAA00, 0x310230, 0x000764, 0x000764};
  build_gradient(pos, col, 7);
}

void load_palette_mono(void)
{
  int i;
  palette[0] = 0x000000;
  for (i = 1; i < 256; i++)
  {
    int v = i;
    palette[i] = (v << 16) | (v << 8) | v;
  }
}

void load_palette(int index)
{
  switch (index)
  {
    case 0: load_palette_classic(); break;
    case 1: load_palette_fire(); break;
    case 2: load_palette_ice(); break;
    case 3: load_palette_ultra(); break;
    case 4: load_palette_mono(); break;
    default: load_palette_classic(); break;
  }
}

void apply_palette(void)
{
  pixpal_load_all(palette);
}

// ---- Upscale center 90% of backbuffer to framebuffer ----
void upscale_to_fb(void)
{
  int ox, oy, sx, sy;

  for (oy = 0; oy < SCREEN_HEIGHT; oy++)
  {
    sy = UPSCALE_Y0 + (oy * UPSCALE_H) / SCREEN_HEIGHT;
    for (ox = 0; ox < SCREEN_WIDTH; ox++)
    {
      sx = UPSCALE_X0 + (ox * UPSCALE_W) / SCREEN_WIDTH;
      __builtin_storeb(PIXEL_FB_ADDR + (oy * SCREEN_WIDTH + ox),
                       backbuf[sy * SCREEN_WIDTH + sx] & 0xFF);
    }
  }
}

// ---- Render one full frame to backbuffer ----
void render_frame(void)
{
  int step_hi;
  unsigned int step_lo;
  int start_re_hi;
  unsigned int start_re_lo;
  int y;
  int x;
  int iter;
  int px;

  // Compute pixel step = scale / 320
  // 1/320 in Q32.32: {0, 0x00CCCCCD}
  mbrot_load_f6(0, 0x00CCCCCD);
  mbrot_load_f7(scale_hi, scale_lo);
  mbrot_mul_f7_f6();  // f7 = scale * (1/320)

  step_hi = mbrot_store_hi_f7();
  step_lo = mbrot_store_lo_f7();

  // Load step into f7 for advance operations
  mbrot_load_step(step_hi, step_lo);

  // Compute start_re = center_re - scale * 0.5
  mbrot_load_f6(0, 0x80000000);  // 0.5
  mbrot_load_f7(scale_hi, scale_lo);
  mbrot_mul_f7_f6();  // f7 = scale * 0.5 = half_width
  {
    int hw_hi = mbrot_store_hi_f7();
    unsigned int hw_lo = mbrot_store_lo_f7();
    mbrot_load_f6(hw_hi, hw_lo);
  }
  mbrot_load_cre(center_re_hi, center_re_lo);  // f0 = center_re
  mbrot_sub_f0_f6();  // f0 = center_re - half_width = start_re

  start_re_hi = mbrot_store_hi_cre();
  start_re_lo = mbrot_store_lo_cre();

  // Compute start_im = center_im - step * 120
  mbrot_load_f6(120, 0);
  mbrot_load_f7(step_hi, step_lo);
  mbrot_mul_f7_f6();  // f7 = step * 120
  {
    int off_hi = mbrot_store_hi_f7();
    unsigned int off_lo = mbrot_store_lo_f7();
    mbrot_load_f6(off_hi, off_lo);
  }
  mbrot_load_cim(center_im_hi, center_im_lo);  // f1 = center_im
  mbrot_sub_f1_f6();  // f1 = center_im - offset = start_im

  for (y = 0; y < SCREEN_HEIGHT; y++)
  {
    // Reset c_re to start_re for this row
    mbrot_load_cre(start_re_hi, start_re_lo);

    for (x = 0; x < SCREEN_WIDTH; x++)
    {
      iter = mbrot_mandelbrot_pixel(max_iter);

      if (iter == 0)
      {
        px = 0;
      }
      else
      {
        px = ((iter - 1) % 255) + 1;
      }

      backbuf[y * SCREEN_WIDTH + x] = px;
      __builtin_storeb(PIXEL_FB_ADDR + (y * SCREEN_WIDTH + x), px);

      // Advance c_re by step
      mbrot_load_step(step_hi, step_lo);
      mbrot_advance_cre();
    }

    // Advance c_im by step
    mbrot_load_step(step_hi, step_lo);
    mbrot_advance_cim();
  }
}

// ---- Compute max iterations from current zoom scale ----
void update_max_iter(void)
{
  int iter;

  if (scale_hi > 0)
  {
    max_iter = 60;
    return;
  }

  if      (scale_lo > 0x40000000) iter = 100;
  else if (scale_lo > 0x10000000) iter = 120;
  else if (scale_lo > 0x04000000) iter = 160;
  else if (scale_lo > 0x01000000) iter = 200;
  else if (scale_lo > 0x00400000) iter = 240;
  else if (scale_lo > 0x00100000) iter = 280;
  else if (scale_lo > 0x00040000) iter = 300;
  else if (scale_lo > 0x00010000) iter = 320;
  else if (scale_lo > 0x00004000) iter = 340;
  else if (scale_lo > 0x00001000) iter = 380;
  else                            iter = 410;

  max_iter = iter;
}

// ---- Reset view to initial state ----
void reset_view(void)
{
  center_re_hi = -1;
  center_re_lo = 0x41A1BEB0;
  center_im_hi = 0;
  center_im_lo = 0x21C01C90;
  scale_hi = 3;
  scale_lo = 0x80000000;
  update_max_iter();
}

int main(void)
{
  int key;
  int keys;
  int running;
  int ci;

  // Open /dev/pixpal (palette DAC) and raw /dev/tty (key events)
  g_pixpal_fd = sys_open("/dev/pixpal", O_WRONLY);
  if (g_pixpal_fd < 0)
  {
    sys_putstr("mbrot: cannot open /dev/pixpal\n");
    return 1;
  }
  g_tty_fd = sys_tty_open_raw(1);
  if (g_tty_fd < 0)
  {
    sys_putstr("mbrot: cannot open /dev/tty in raw mode\n");
    sys_close(g_pixpal_fd);
    return 1;
  }

  // Clear terminal and pixel framebuffer
  term_clear();
  for (ci = 0; ci < SCREEN_WIDTH * SCREEN_HEIGHT; ci++)
  {
    __builtin_storeb(PIXEL_FB_ADDR + ci, 0);
  }

  // Allocate off-screen backbuffer (1 byte per pixel)
  backbuf = (char *)sys_heap_alloc(SCREEN_WIDTH * SCREEN_HEIGHT);
  for (ci = 0; ci < SCREEN_WIDTH * SCREEN_HEIGHT; ci++)
  {
    backbuf[ci] = 0;
  }

  // Set up initial view
  reset_view();
  auto_zoom = 1;

  // Load and apply palette
  current_palette = 3;
  load_palette(current_palette);
  apply_palette();

  // ---- Main render loop ----
  running = 1;
  while (running)
  {
    // Check keyboard input
    while ((key = sys_tty_event_read(g_tty_fd, 0)) >= 0)
    {
      if (key == ' ')
      {
        auto_zoom = !auto_zoom;
      }
      else if (key == 'r' || key == 'R')
      {
        reset_view();
      }
      else if (key == 'p' || key == 'P')
      {
        current_palette = (current_palette + 1) % NUM_PALETTES;
        load_palette(current_palette);
        apply_palette();
      }
    }

    keys = sys_get_key_state();
    if (keys & KEYSTATE_ESCAPE)
    {
      running = 0;
      break;
    }

    if (auto_zoom)
    {
      // Advance zoom: scale *= 0.90
      mbrot_load_f6(scale_hi, scale_lo);
      mbrot_load_f7(0, ZOOM_FACTOR_LO);
      mbrot_mul_f7_f6();  // f7 = scale * 0.90
      scale_hi = mbrot_store_hi_f7();
      scale_lo = mbrot_store_lo_f7();
      update_max_iter();

      // Show zoom preview of previous frame while new frame renders
      upscale_to_fb();
    }

    render_frame();

    if (!auto_zoom)
    {
      sys_delay(50);
    }

    // Check zoom limit
    if (auto_zoom && scale_hi == 0 && scale_lo < MIN_SCALE_LO)
    {
      sys_delay(3000);
      reset_view();
    }
  }

  // Cleanup: restore default RRRGGGBB palette and clear pixel framebuffer
  {
    int i;
    int defpal[256];
    for (i = 0; i < 256; i++)
    {
      int r3;
      int g3;
      int b2;
      int r;
      int g;
      int b;

      r3 = (i >> 5) & 7;
      g3 = (i >> 2) & 7;
      b2 = i & 3;

      r = (r3 << 5) | (r3 << 2) | (r3 >> 1);
      g = (g3 << 5) | (g3 << 2) | (g3 >> 1);
      b = (b2 << 6) | (b2 << 4) | (b2 << 2) | b2;

      defpal[i] = (r << 16) | (g << 8) | b;
    }
    pixpal_load_all(defpal);
  }
  {
    int ci;
    for (ci = 0; ci < SCREEN_WIDTH * SCREEN_HEIGHT; ci++)
    {
      __builtin_storeb(PIXEL_FB_ADDR + ci, 0);
    }
  }
  term_clear();

  sys_close(g_tty_fd);
  sys_close(g_pixpal_fd);
  return 0;
}
