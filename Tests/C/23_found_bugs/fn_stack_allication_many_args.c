#define GPU_PIXEL_DATA_ADDR         0x7B00000 // Pixel plane data

void gpu_write_pixel_data(unsigned int x, unsigned int y, unsigned int color)
{
  // Pixel plane is 320x240 pixels, we wrap around if out of bounds to prevent out-of-bounds writes
  if (x >= 320)
  {
    x = x % 320;
  }

  if (y >= 240)
  {
    y = y % 240;
  }

  unsigned int *pixel_plane = (unsigned int *)GPU_PIXEL_DATA_ADDR;
  unsigned int index = (y * 320) + x;
  pixel_plane[index] = color;
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

int main() {
    //int color = 0; // Uncomment this line to not trigger the bug!

    fb_fill_rect(10, 10, 3, 3, 0xAA);

    fb_draw_line(12, 12, 10, 10, 0x03);

    return 0x39; // expected=0x39
}

void interrupt()
{
    // No need to handle interrupts
}
