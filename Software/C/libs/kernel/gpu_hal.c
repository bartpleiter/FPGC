/*
 * GPU Hardware Abstraction Layer (HAL)
 * Provides an abstraction for GPU VRAM memory access.
 * If more speed is needed, inline assembly could be used in these functions.
 * For writing entire frames to the pixel plane, consider not using this library and accessing VRAM directly.
 */

// GPU VRAM memory addresses
#define GPU_PATTERN_TABLE_ADDR      0x7900000 // 256 patterns × 4 words each
#define GPU_PALETTE_TABLE_ADDR      0x7900400 // 32 palettes × 1 word each
#define GPU_BG_WINDOW_TILE_ADDR     0x7A00000 // Background plane tile table
#define GPU_BG_WINDOW_COLOR_ADDR    0x7A00800 // Background plane color table
#define GPU_WINDOW_TILE_ADDR        0x7A01000 // Window plane tile table
#define GPU_WINDOW_COLOR_ADDR       0x7A01800 // Window plane color table
#define GPU_PARAMETERS_ADDR         0x7A02000 // GPU parameters
#define GPU_PIXEL_DATA_ADDR         0x7B00000 // Pixel plane data

void gpu_clear_vram()
{
  int i;

  // Clear pattern and palette tables
  unsigned int *vram_ptr = (unsigned int *)GPU_PATTERN_TABLE_ADDR;
  for (i = 0; i < 1024; i++)
  {
    vram_ptr[i] = 0;
  }
  vram_ptr = (unsigned int *)GPU_PALETTE_TABLE_ADDR;
  for (i = 0; i < 32; i++)
  {
    vram_ptr[i] = 0;
  }

  // Clear background and window tile/color tables
  vram_ptr = (unsigned int *)GPU_BG_WINDOW_TILE_ADDR;
  for (i = 0; i < (40 * 25); i++)
  {
    vram_ptr[i] = 0;
  }
  vram_ptr = (unsigned int *)GPU_BG_WINDOW_COLOR_ADDR;
  for (i = 0; i < (40 * 25); i++)
  {
    vram_ptr[i] = 0;
  }
  vram_ptr = (unsigned int *)GPU_WINDOW_TILE_ADDR;
  for (i = 0; i < (60 * 25); i++)
  {
    vram_ptr[i] = 0;
  }
  vram_ptr = (unsigned int *)GPU_WINDOW_COLOR_ADDR;
  for (i = 0; i < (60 * 25); i++)
  {
    vram_ptr[i] = 0;
  }

  // Clear parameters
  vram_ptr = (unsigned int *)GPU_PARAMETERS_ADDR;
  for (i = 0; i < 2; i++)
  {
    vram_ptr[i] = 0;
  }

  // Clear pixel data
  vram_ptr = (unsigned int *)GPU_PIXEL_DATA_ADDR;
  for (i = 0; i < (320 * 240); i++)
  {
    vram_ptr[i] = 0;
  }
}

void gpu_load_pattern_table(const unsigned int* pattern_table)
{
  int i;
  unsigned int *vram_pattern_table = (unsigned int *)GPU_PATTERN_TABLE_ADDR;

  // Pattern table is 1024 words (256 patterns × 4 words each)
  for (i = 0; i < 1024; i++)
  {
    vram_pattern_table[i] = pattern_table[i];
  }
}

void gpu_load_palette_table(const unsigned int* palette_table)
{
  int i;
  unsigned int *vram_palette_table = (unsigned int *)GPU_PALETTE_TABLE_ADDR;

  // Palette table is 32 words (32 palettes × 1 word each)
  for (i = 0; i < 32; i++)
  {
    vram_palette_table[i] = palette_table[i];
  }
}

// Scrolls the background plane by tile units to the left
void gpu_set_bg_tile_scroll(unsigned int tile_x)
{
  unsigned int *params = (unsigned int *)GPU_PARAMETERS_ADDR;
  params[0] = tile_x;
}

// Scrolls the background plane by pixel units to the left (0-7 within tile)
void gpu_set_bg_pixel_scroll(unsigned int pixel_x)
{
  unsigned int *params = (unsigned int *)GPU_PARAMETERS_ADDR;
  params[1] = pixel_x;
}

// Set palette for the entire window plane
void gpu_set_window_palette(unsigned int palette_index)
{
  int i;
  unsigned int *window_color_table = (unsigned int *)GPU_WINDOW_COLOR_ADDR;
  for (i = 0; i < (40 * 25); i++)
  {
    window_color_table[i] = palette_index;
  }
}

// Set palette for the entire background plane
void gpu_set_bg_palette(unsigned int palette_index)
{
  int i;
  unsigned int *bg_color_table = (unsigned int *)GPU_BG_WINDOW_COLOR_ADDR;
  for (i = 0; i < (64 * 25); i++)
  {
    bg_color_table[i] = palette_index;
  }
}

void gpu_write_window_tile(unsigned int x, unsigned int y, unsigned int tile_index, unsigned int palette_index)
{
  // The window plane is 40x25 tiles, we wrap around if out of bounds to prevent out-of-bounds writes
  if (x >= 40)
  {
    x = x % 40;
  }

  if (y >= 25)
  {
    y = y % 25;
  }

  unsigned int *window_tile_table = (unsigned int *)GPU_WINDOW_TILE_ADDR;
  unsigned int *window_color_table = (unsigned int *)GPU_WINDOW_COLOR_ADDR;
  unsigned int index = (y * 40) + x;

  window_tile_table[index] = tile_index;
  window_color_table[index] = palette_index;
}

void gpu_write_bg_tile(unsigned int x, unsigned int y, unsigned int tile_index, unsigned int palette_index)
{
  // Background plane is 64x25 tiles, we wrap around if out of bounds to prevent out-of-bounds writes
  if (x >= 64)
  {
    x = x % 64;
  }

  if (y >= 25)
  {
    y = y % 25;
  }

  unsigned int *bg_tile_table = (unsigned int *)GPU_BG_WINDOW_TILE_ADDR;
  unsigned int *bg_color_table = (unsigned int *)GPU_BG_WINDOW_COLOR_ADDR;
  unsigned int index = (y * 64) + x;

  bg_tile_table[index] = tile_index;
  bg_color_table[index] = palette_index;
}

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