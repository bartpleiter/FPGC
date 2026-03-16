//
// plot.c — Simple line plotting library for FPGC VRAMPX.
//

#include "libs/user/gfx/plot.h"

// ---- Pixel framebuffer ----
#define PLOT_FB_ADDR   0x1EC00000
#define PLOT_FB_WIDTH  320
#define PLOT_FB_HEIGHT 240

// ---- Current plot region ----
int plot_x;
int plot_y;
int plot_w;
int plot_h;

// Margin inside the plot region for axes/labels (in pixels)
#define PLOT_MARGIN_LEFT   20
#define PLOT_MARGIN_RIGHT  2
#define PLOT_MARGIN_TOP    8
#define PLOT_MARGIN_BOTTOM 8

// ---- 3x5 mini-font ----
// Packed: val = (row0<<12)|(row1<<9)|(row2<<6)|(row3<<3)|row4
// Row bits: bit2=left, bit1=center, bit0=right

// Font table indexed by ASCII code 32..90 (space through Z)
int plot_font[59] = {
  0,      // 32 ' '
  0,0,0,0,0,0,0,0,0,0,0,0, // 33-44 (unused punctuation)
  448,    // 45 '-'
  2,      // 46 '.'
  4772,   // 47 '/'
  31599,  // 48 '0'
  11415,  // 49 '1'
  29671,  // 50 '2'
  29647,  // 51 '3'
  23497,  // 52 '4'
  31183,  // 53 '5'
  31215,  // 54 '6'
  29257,  // 55 '7'
  31727,  // 56 '8'
  31695,  // 57 '9'
  1040,   // 58 ':'
  0,0,0,0,0,0, // 59-64 (unused)
  11245,  // 65 'A'
  27566,  // 66 'B'
  14627,  // 67 'C'
  27502,  // 68 'D'
  31207,  // 69 'E'
  31140,  // 70 'F'
  14699,  // 71 'G'
  23533,  // 72 'H'
  29847,  // 73 'I'
  4714,   // 74 'J'
  23469,  // 75 'K'
  18727,  // 76 'L'
  24557,  // 77 'M'
  27501,  // 78 'N'
  31599,  // 79 'O'
  27556,  // 80 'P'
  11089,  // 81 'Q'
  27565,  // 82 'R'
  14478,  // 83 'S'
  29842,  // 84 'T'
  23407,  // 85 'U'
  23402,  // 86 'V'
  23549,  // 87 'W'
  23213,  // 88 'X'
  23186,  // 89 'Y'
  29351   // 90 'Z'
};

// ---- Internal pixel helpers ----

void plot_put_pixel(int x, int y, int color)
{
  unsigned int *fb;
  if (x < 0 || x >= PLOT_FB_WIDTH || y < 0 || y >= PLOT_FB_HEIGHT) return;
  fb = (unsigned int *)PLOT_FB_ADDR;
  fb[y * PLOT_FB_WIDTH + x] = color;
}

void plot_hline(int x0, int x1, int y, int color)
{
  int x;
  for (x = x0; x <= x1; x++)
  {
    plot_put_pixel(x, y, color);
  }
}

void plot_vline(int x, int y0, int y1, int color)
{
  int y;
  for (y = y0; y <= y1; y++)
  {
    plot_put_pixel(x, y, color);
  }
}

// Bresenham line drawing
void plot_draw_line(int x0, int y0, int x1, int y1, int color)
{
  int dx;
  int dy;
  int sx;
  int sy;
  int err;
  int e2;

  if (x0 < x1) { dx = x1 - x0; sx = 1; }
  else          { dx = x0 - x1; sx = -1; }

  if (y0 < y1) { dy = y1 - y0; sy = 1; }
  else          { dy = y0 - y1; sy = -1; }

  if (dx > dy) { err = dx / 2; }
  else         { err = 0 - dy / 2; }

  while (1)
  {
    plot_put_pixel(x0, y0, color);
    if (x0 == x1 && y0 == y1) break;
    e2 = err;
    if (e2 > (0 - dx))
    {
      err = err - dy;
      x0 = x0 + sx;
    }
    if (e2 < dy)
    {
      err = err + dx;
      y0 = y0 + sy;
    }
  }
}

// ---- Font rendering ----

void plot_draw_char(int px, int py, int ch, int color)
{
  int font_idx;
  int packed;
  int row;
  int col;
  int row_bits;

  // Map lowercase to uppercase
  if (ch >= 97 && ch <= 122) ch = ch - 32;

  font_idx = ch - 32;
  if (font_idx < 0 || font_idx > 58) return;

  packed = plot_font[font_idx];
  if (packed == 0 && ch != 32) return;

  for (row = 0; row < 5; row++)
  {
    row_bits = (packed >> (12 - row * 3)) & 7;
    for (col = 0; col < 3; col++)
    {
      if (row_bits & (4 >> col))
      {
        plot_put_pixel(px + col, py + row, color);
      }
    }
  }
}

void plot_text(int x, int y, char *str, int color)
{
  int i;
  i = 0;
  while (str[i] != 0)
  {
    plot_draw_char(x, y, str[i], color);
    x = x + 4; // 3px char + 1px spacing
    i = i + 1;
  }
}

void plot_number(int x, int y, int val, int color)
{
  char buf[12];
  int i;
  int neg;
  unsigned int uval;

  if (val == 0)
  {
    plot_draw_char(x, y, '0', color);
    return;
  }

  neg = 0;
  if (val < 0)
  {
    neg = 1;
    uval = (unsigned int)(0 - val);
  }
  else
  {
    uval = (unsigned int)val;
  }

  i = 11;
  buf[i] = 0;
  while (uval > 0)
  {
    i = i - 1;
    buf[i] = '0' + (uval % 10);
    uval = uval / 10;
  }
  if (neg)
  {
    i = i - 1;
    buf[i] = '-';
  }
  plot_text(x, y, buf + i, color);
}

// ---- Plot functions ----

void plot_init(int x, int y, int w, int h)
{
  plot_x = x;
  plot_y = y;
  plot_w = w;
  plot_h = h;
}

void plot_clear(int bg_color)
{
  int px;
  int py;
  unsigned int *fb;
  fb = (unsigned int *)PLOT_FB_ADDR;
  for (py = plot_y; py < plot_y + plot_h; py++)
  {
    if (py >= 0 && py < PLOT_FB_HEIGHT)
    {
      for (px = plot_x; px < plot_x + plot_w; px++)
      {
        if (px >= 0 && px < PLOT_FB_WIDTH)
        {
          fb[py * PLOT_FB_WIDTH + px] = bg_color;
        }
      }
    }
  }
}

void plot_axes(int y_min, int y_max, int x_max, int axis_color, int grid_color)
{
  int data_x0;
  int data_y0;
  int data_x1;
  int data_y1;
  int data_w;
  int data_h;
  int nticks;
  int i;
  int tick_val;
  int tick_y;
  int step;

  data_x0 = plot_x + PLOT_MARGIN_LEFT;
  data_y0 = plot_y + PLOT_MARGIN_TOP;
  data_x1 = plot_x + plot_w - 1 - PLOT_MARGIN_RIGHT;
  data_y1 = plot_y + plot_h - 1 - PLOT_MARGIN_BOTTOM;
  data_w = data_x1 - data_x0;
  data_h = data_y1 - data_y0;

  // Draw Y axis
  plot_vline(data_x0, data_y0, data_y1, axis_color);
  // Draw X axis
  plot_hline(data_x0, data_x1, data_y1, axis_color);

  // Y axis tick marks and labels (4 ticks)
  nticks = 4;
  if (y_max <= y_min) return;

  step = (y_max - y_min) / nticks;
  if (step < 1) step = 1;

  for (i = 0; i <= nticks; i++)
  {
    tick_val = y_min + i * step;
    if (tick_val > y_max) tick_val = y_max;

    // Map tick_val to pixel Y (inverted: y_max at top, y_min at bottom)
    tick_y = data_y1 - ((tick_val - y_min) * data_h) / (y_max - y_min);

    // Tick mark
    plot_hline(data_x0 - 2, data_x0, tick_y, axis_color);

    // Grid line
    if (grid_color != 0 && i > 0 && i < nticks)
    {
      int gx;
      for (gx = data_x0 + 2; gx <= data_x1; gx = gx + 4)
      {
        plot_put_pixel(gx, tick_y, grid_color);
      }
    }

    // Label (right-aligned before the axis)
    plot_number(plot_x + 1, tick_y - 2, tick_val, axis_color);
  }
}

void plot_line(int *data, int count, int y_min, int y_max, int color)
{
  int data_x0;
  int data_y0;
  int data_x1;
  int data_y1;
  int data_w;
  int data_h;
  int i;
  int px0;
  int py0;
  int px1;
  int py1;
  int val;
  int range;

  if (count < 2) return;
  if (y_max <= y_min) return;

  data_x0 = plot_x + PLOT_MARGIN_LEFT;
  data_y0 = plot_y + PLOT_MARGIN_TOP;
  data_x1 = plot_x + plot_w - 1 - PLOT_MARGIN_RIGHT;
  data_y1 = plot_y + plot_h - 1 - PLOT_MARGIN_BOTTOM;
  data_w = data_x1 - data_x0;
  data_h = data_y1 - data_y0;
  range = y_max - y_min;

  // First point
  val = data[0];
  if (val < y_min) val = y_min;
  if (val > y_max) val = y_max;
  px0 = data_x0;
  py0 = data_y1 - ((val - y_min) * data_h) / range;

  for (i = 1; i < count; i++)
  {
    val = data[i];
    if (val < y_min) val = y_min;
    if (val > y_max) val = y_max;

    px1 = data_x0 + (i * data_w) / (count - 1);
    py1 = data_y1 - ((val - y_min) * data_h) / range;

    plot_draw_line(px0, py0, px1, py1, color);

    px0 = px1;
    py0 = py1;
  }
}
