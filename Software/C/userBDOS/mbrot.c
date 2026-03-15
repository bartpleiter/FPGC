//
// mbrot.c — Standalone Mandelbrot zoom animation (userBDOS).
// Renders directly on a single device using the FP64 coprocessor.
// Comparison baseline for the cluster demo (mbrot_host/mbrot_client).
//

#define USER_SYSCALL
#include "libs/user/user.h"

// ---- Screen constants ----
#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240
#define PIXEL_FB_ADDR 0x1EC00000

unsigned int *fb = (unsigned int *)PIXEL_FB_ADDR;

// ---- FP64 register allocation ----
#define F_CRE  0
#define F_CIM  1
#define F_ZRE  2
#define F_ZIM  3
#define F_TMP1 4
#define F_TMP2 5
#define F_TMP3 6
#define F_STEP 7

// ---- Palette data ----
#define NUM_PALETTES 5
int current_palette;
int palette[256];

// ---- View state (Q32.32 fixed-point) ----
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

void load_palette_classic()
{
  int pos[6]   = {1,   51,  102,  153,  204,  255};
  int col[6] = {0x000080, 0x00FFFF, 0x00FF00, 0xFFFF00, 0xFF0000, 0x000000};
  build_gradient(pos, col, 6);
}

void load_palette_fire()
{
  int pos[5]   = {1,   64,  128,  192,  255};
  int col[5] = {0x200000, 0xAA0000, 0xFF6600, 0xFFFF00, 0xFFFFFF};
  build_gradient(pos, col, 5);
}

void load_palette_ice()
{
  int pos[4]   = {1,   85,  170,  255};
  int col[4] = {0x000020, 0x0000CC, 0x00CCFF, 0xFFFFFF};
  build_gradient(pos, col, 4);
}

void load_palette_ultra()
{
  int pos[7]   = {1,   42,  85,  128,  170,  213,  255};
  int col[7] = {0x000764, 0x206BCB, 0xEDFFFF, 0xFFAA00, 0x310230, 0x000764, 0x000764};
  build_gradient(pos, col, 7);
}

void load_palette_mono()
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

void apply_palette()
{
  int i;
  for (i = 0; i < 256; i++)
  {
    sys_set_pixel_palette(i, palette[i]);
  }
}

// ---- Mandelbrot iteration for one pixel ----
// Assumes F_CRE (f0) and F_CIM (f1) are loaded with the complex coordinate.
// Returns iteration count (0 = in set, 1..max_iter = escaped).
int mandelbrot_pixel()
{
  int retval = 0;
  asm(
    "addr2reg Label_max_iter r11"
    "read 0 r11 r4"

    "fld r0 r0 r2"
    "fld r0 r0 r3"

    "or r0 r0 r5"

    "beq r4 r0 Label_mbrot_asm_set"

    "Label_mbrot_asm_loop:"
    "fmul r2 r2 r4       ; f4 = z_re^2"
    "fmul r3 r3 r5       ; f5 = z_im^2"
    "fmul r2 r3 r6       ; f6 = z_re * z_im"
    "fsub r4 r5 r2       ; f2 = z_re^2 - z_im^2"
    "fadd r2 r0 r2       ; f2 += c_re  (new z_re)"
    "fadd r6 r6 r3       ; f3 = 2 * z_re * z_im"
    "fadd r3 r1 r3       ; f3 += c_im  (new z_im)"

    "fadd r4 r5 r4       ; f4 = |z|^2"
    "fsthi r4 r0 r1      ; r1 = integer part of |z|^2"
    "sltu r1 4 r1        ; r1 = 1 if mag < 4 (not escaped)"
    "beq r1 r0 Label_mbrot_asm_escaped"

    "add r5 1 r5"
    "slt r5 r4 r1        ; iter < max_iter?"
    "bne r1 r0 Label_mbrot_asm_loop"

    "Label_mbrot_asm_set:"
    "write -4 r14 r0     ; retval = 0"
    "jump Label_mbrot_asm_done"

    "Label_mbrot_asm_escaped:"
    "add r5 1 r1         ; r1 = iter + 1"
    "write -4 r14 r1     ; retval = iter + 1"

    "Label_mbrot_asm_done:"
  );
  return retval;
}

// ---- Render one full frame to framebuffer ----
void render_frame()
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
  __fld(6, 0, 0x00CCCCCD);
  __fld(F_STEP, scale_hi, scale_lo);
  __fmul(F_STEP, F_STEP, 6);

  step_hi = __fsthi(F_STEP);
  step_lo = __fstlo(F_STEP);

  // start_re = center_re - scale * 0.5
  __fld(F_TMP2, 0, 0x80000000);
  __fld(6, scale_hi, scale_lo);
  __fmul(6, 6, F_TMP2);
  __fld(F_CRE, center_re_hi, center_re_lo);
  __fsub(F_CRE, F_CRE, 6);

  start_re_hi = __fsthi(F_CRE);
  start_re_lo = __fstlo(F_CRE);

  // start_im = center_im - step * 120 (half height)
  __fld(6, 120, 0);
  __fld(F_STEP, step_hi, step_lo);
  __fmul(6, F_STEP, 6);
  __fld(F_CIM, center_im_hi, center_im_lo);
  __fsub(F_CIM, F_CIM, 6);

  for (y = 0; y < SCREEN_HEIGHT; y++)
  {
    // Reset c_re to start_re for this row
    __fld(F_CRE, start_re_hi, start_re_lo);

    for (x = 0; x < SCREEN_WIDTH; x++)
    {
      iter = mandelbrot_pixel();

      if (iter == 0)
      {
        px = 0;
      }
      else
      {
        px = ((iter - 1) % 255) + 1;
      }

      fb[y * SCREEN_WIDTH + x] = px;

      // Advance c_re by step
      __fld(F_STEP, step_hi, step_lo);
      __fadd(F_CRE, F_CRE, F_STEP);
    }

    // Advance c_im by step
    __fld(F_STEP, step_hi, step_lo);
    __fadd(F_CIM, F_CIM, F_STEP);
  }
}

// ---- Compute max iterations from current zoom scale ----
void update_max_iter()
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
void reset_view()
{
  center_re_hi = -1;
  center_re_lo = 0x41A1BEB0;
  center_im_hi = 0;
  center_im_lo = 0x21C01C90;

  scale_hi = 3;
  scale_lo = 0x80000000;

  update_max_iter();
}

int main()
{
  int key;
  int keys;
  int running;

  // Clear terminal and pixel framebuffer
  sys_term_clear();
  {
    int ci;
    for (ci = 0; ci < SCREEN_WIDTH * SCREEN_HEIGHT; ci++)
    {
      fb[ci] = 0;
    }
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

    // Check escape key
    keys = sys_get_key_state();
    if (keys & KEYSTATE_ESCAPE)
    {
      running = 0;
      break;
    }

    // Render the current frame
    render_frame();

    if (!auto_zoom)
    {
      sys_delay(50);
      continue;
    }

    // Advance zoom: scale *= 0.90
    __fld(6, scale_hi, scale_lo);
    __fld(7, 0, ZOOM_FACTOR_LO);
    __fmul(6, 6, 7);
    scale_hi = __fsthi(6);
    scale_lo = __fstlo(6);

    update_max_iter();

    // Check zoom limit
    if (scale_hi == 0 && scale_lo < MIN_SCALE_LO)
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
      fb[ci] = 0;
    }
  }
  sys_term_clear();

  return 0;
}
