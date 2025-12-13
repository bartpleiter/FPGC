#include "libs/kernel/gfx/gpu_fb.h"

void fb_clear()
{
  gpu_clear_pixel();
}

void fb_set_pixel(unsigned int x, unsigned int y, unsigned int color)
{
  gpu_write_pixel_data(x, y, color);
}

void fb_draw_line(int x0, int y0, int x1, int y1, unsigned int color)
{
  int dx = x1 > x0 ? x1 - x0 : x0 - x1;
  int dy = y1 > y0 ? y1 - y0 : y0 - y1;
  int sx = x0 < x1 ? 1 : -1;
  int sy = y0 < y1 ? 1 : -1;
  int err = dx - dy;
  int e2;

  while (1)
  {
    gpu_write_pixel_data(x0, y0, color);

    if (x0 == x1 && y0 == y1)
      break;

    e2 = 2 * err;
    if (e2 > -dy)
    {
      err -= dy;
      x0 += sx;
    }
    if (e2 < dx)
    {
      err += dx;
      y0 += sy;
    }
  }
}

void fb_draw_rect(int x, int y, int w, int h, unsigned int color)
{
  // Draw rectangle outline using four lines
  if (w == 0 || h == 0)
    return;

  // Top edge
  fb_draw_line(x, y, x + w - 1, y, color);
  // Bottom edge
  fb_draw_line(x, y + h - 1, x + w - 1, y + h - 1, color);
  // Left edge
  fb_draw_line(x, y, x, y + h - 1, color);
  // Right edge
  fb_draw_line(x + w - 1, y, x + w - 1, y + h - 1, color);
}

void fb_fill_rect(unsigned int x, unsigned int y, unsigned int w, unsigned int h, unsigned int color)
{
  // Draw filled rectangle by setting each pixel
  unsigned int i, j;
  for (j = 0; j < h; j++)
  {
    for (i = 0; i < w; i++)
    {
      gpu_write_pixel_data(x + i, y + j, color);
    }
  }
}

void fb_draw_circle(int x, int y, int radius, unsigned int color)
{
  // Midpoint Circle Algorithm
  int x_pos = radius;
  int y_pos = 0;
  int err = 0;

  while (x_pos >= y_pos)
  {
    // Draw 8 octants
    gpu_write_pixel_data(x + x_pos, y + y_pos, color);
    gpu_write_pixel_data(x + y_pos, y + x_pos, color);
    gpu_write_pixel_data(x - y_pos, y + x_pos, color);
    gpu_write_pixel_data(x - x_pos, y + y_pos, color);
    gpu_write_pixel_data(x - x_pos, y - y_pos, color);
    gpu_write_pixel_data(x - y_pos, y - x_pos, color);
    gpu_write_pixel_data(x + y_pos, y - x_pos, color);
    gpu_write_pixel_data(x + x_pos, y - y_pos, color);

    if (err <= 0)
    {
      y_pos += 1;
      err += 2 * y_pos + 1;
    }

    if (err > 0)
    {
      x_pos -= 1;
      err -= 2 * x_pos + 1;
    }
  }
}

void fb_blit(unsigned int x, unsigned int y, unsigned int width, unsigned int height, const unsigned int *data)
{
  // Copy bitmap data to framebuffer
  unsigned int i, j;
  for (j = 0; j < height; j++)
  {
    for (i = 0; i < width; i++)
    {
      unsigned int pixel_data = data[j * width + i];
      gpu_write_pixel_data(x + i, y + j, pixel_data);
    }
  }
}
