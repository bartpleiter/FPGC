//
// mbrot.c — Standalone Mandelbrot zoom animation (userBDOS).
// Renders directly on a single device using the FP64 coprocessor
// via the fixed64 Q32.32 library.
//

#include <syscall.h>
#include <plot.h>
#include <fixed64.h>

// ---- Screen constants ----
#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240

// ---- Palette data ----
#define NUM_PALETTES 5
int current_palette;
int palette[256];

// ---- View state (Q32.32 fixed-point) ----
struct fp64 center_re;
struct fp64 center_im;
struct fp64 scale;
int max_iter;

// Minimum scale before reset (~1e-7 in Q32.32)
#define MIN_SCALE_LO 0x00001000

// Zoom factor per frame: 0.90 in Q32.32
#define ZOOM_FACTOR_LO 0xE6666666

// ---- Auto-zoom state ----
int auto_zoom;

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
  int i;
  for (i = 0; i < 256; i++)
  {
    sys_set_pixel_palette(i, palette[i]);
  }
}

// ---- Mandelbrot iteration for one pixel ----
// Returns iteration count (0 = in set, 1..max_iter = escaped).
int mandelbrot_pixel(struct fp64 *c_re, struct fp64 *c_im)
{
  struct fp64 z_re;
  struct fp64 z_im;
  struct fp64 zr2;
  struct fp64 zi2;
  struct fp64 zri;
  struct fp64 mag;
  struct fp64 four;
  struct fp64 tmp;
  int i;

  fp64_make(&z_re, 0, 0);
  fp64_make(&z_im, 0, 0);
  fp64_make(&four, 4, 0);

  for (i = 0; i < max_iter; i++)
  {
    fp64_mul(&zr2, &z_re, &z_re);
    fp64_mul(&zi2, &z_im, &z_im);
    fp64_mul(&zri, &z_re, &z_im);

    // z_re = z_re^2 - z_im^2 + c_re
    fp64_sub(&tmp, &zr2, &zi2);
    fp64_add(&z_re, &tmp, c_re);

    // z_im = 2 * z_re_old * z_im + c_im
    fp64_add(&tmp, &zri, &zri);
    fp64_add(&z_im, &tmp, c_im);

    // Check |z|^2 >= 4
    fp64_add(&mag, &zr2, &zi2);
    if (fp64_cmp(&mag, &four) >= 0)
    {
      return i + 1;
    }
  }

  return 0;
}

// ---- Render one full frame to framebuffer ----
void render_frame(void)
{
  struct fp64 step;
  struct fp64 start_re;
  struct fp64 start_im;
  struct fp64 c_re;
  struct fp64 c_im;
  struct fp64 inv320;
  struct fp64 half;
  struct fp64 half_h;
  int y;
  int x;
  int iter;
  int px;

  // step = scale / 320
  // 1/320 in Q32.32: {0, 0x00CCCCCD}
  fp64_make(&inv320, 0, 0x00CCCCCD);
  fp64_mul(&step, &scale, &inv320);

  // start_re = center_re - scale * 0.5
  fp64_make(&half, 0, 0x80000000);
  {
    struct fp64 tmp;
    fp64_mul(&tmp, &scale, &half);
    fp64_sub(&start_re, &center_re, &tmp);
  }

  // start_im = center_im - step * 120
  fp64_from_int(&half_h, 120);
  {
    struct fp64 tmp;
    fp64_mul(&tmp, &step, &half_h);
    fp64_sub(&start_im, &center_im, &tmp);
  }

  c_im = start_im;

  for (y = 0; y < SCREEN_HEIGHT; y++)
  {
    c_re = start_re;

    for (x = 0; x < SCREEN_WIDTH; x++)
    {
      iter = mandelbrot_pixel(&c_re, &c_im);

      if (iter == 0)
      {
        px = 0;
      }
      else
      {
        px = ((iter - 1) % 255) + 1;
      }

      __builtin_store(PIXEL_FB_ADDR + (y * SCREEN_WIDTH + x) * 4, px);

      fp64_add(&c_re, &c_re, &step);
    }

    fp64_add(&c_im, &c_im, &step);
  }
}

// ---- Compute max iterations from current zoom scale ----
void update_max_iter(void)
{
  int iter;

  if (scale.hi > 0)
  {
    max_iter = 60;
    return;
  }

  if      (scale.lo > 0x40000000) iter = 100;
  else if (scale.lo > 0x10000000) iter = 120;
  else if (scale.lo > 0x04000000) iter = 160;
  else if (scale.lo > 0x01000000) iter = 200;
  else if (scale.lo > 0x00400000) iter = 240;
  else if (scale.lo > 0x00100000) iter = 280;
  else if (scale.lo > 0x00040000) iter = 300;
  else if (scale.lo > 0x00010000) iter = 320;
  else if (scale.lo > 0x00004000) iter = 340;
  else if (scale.lo > 0x00001000) iter = 380;
  else                            iter = 410;

  max_iter = iter;
}

// ---- Reset view to initial state ----
void reset_view(void)
{
  fp64_make(&center_re, -1, 0x41A1BEB0);
  fp64_make(&center_im, 0, 0x21C01C90);
  fp64_make(&scale, 3, 0x80000000);
  update_max_iter();
}

int main(void)
{
  int key;
  int keys;
  int running;
  struct fp64 zoom_factor;

  // Clear terminal and pixel framebuffer
  sys_term_clear();
  {
    int ci;
    for (ci = 0; ci < SCREEN_WIDTH * SCREEN_HEIGHT; ci++)
    {
      __builtin_store(PIXEL_FB_ADDR + ci * 4, 0);
    }
  }

  // Set up initial view
  reset_view();
  auto_zoom = 1;

  // Load and apply palette
  current_palette = 3;
  load_palette(current_palette);
  apply_palette();

  fp64_make(&zoom_factor, 0, ZOOM_FACTOR_LO);

  // ---- Main render loop ----
  running = 1;
  while (running)
  {
    // Check keyboard input
    while (sys_key_available())
    {
      key = sys_read_key();
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

    render_frame();

    if (!auto_zoom)
    {
      sys_delay(50);
      continue;
    }

    // Advance zoom: scale *= 0.90
    fp64_mul(&scale, &scale, &zoom_factor);
    update_max_iter();

    // Check zoom limit
    if (scale.hi == 0 && scale.lo < MIN_SCALE_LO)
    {
      sys_delay(3000);
      reset_view();
    }
  }

  // Cleanup: restore default RRRGGGBB palette and clear pixel framebuffer
  {
    int i;
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

      sys_set_pixel_palette(i, (r << 16) | (g << 8) | b);
    }
  }
  {
    int ci;
    for (ci = 0; ci < SCREEN_WIDTH * SCREEN_HEIGHT; ci++)
    {
      __builtin_store(PIXEL_FB_ADDR + ci * 4, 0);
    }
  }
  sys_term_clear();

  return 0;
}
