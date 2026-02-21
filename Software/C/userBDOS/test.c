// Very simple user test program that verifies if code execution works
// without using system calls, since those are not implemented yet as of writing.

#define GPU_PIXEL_DATA_ADDR 0x7B00000      // Pixel plane data

// Write one pixel value in the pixel plane.
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

int main()
{
  // Draw a single pixel
  gpu_write_pixel_data(0, 0, 0xFF); // White at top left corner
  return 37;
}
