#ifndef GPU_HAL_H
#define GPU_HAL_H

// GPU Hardware Abstraction Layer (HAL)
// Provides an abstraction for GPU VRAM memory access.
// If more speed is needed, inline assembly could be used in these functions.
// For writing entire frames to the pixel plane, consider not using this library and accessing VRAM directly.

// GPU VRAM memory addresses
#define GPU_PATTERN_TABLE_ADDR 0x7900000   // 256 patterns × 4 words each
#define GPU_PALETTE_TABLE_ADDR 0x7900400   // 32 palettes × 1 word each
#define GPU_BG_WINDOW_TILE_ADDR 0x7A00000  // Background plane tile table
#define GPU_BG_WINDOW_COLOR_ADDR 0x7A00800 // Background plane color table
#define GPU_WINDOW_TILE_ADDR 0x7A01000     // Window plane tile table
#define GPU_WINDOW_COLOR_ADDR 0x7A01800    // Window plane color table
#define GPU_PARAMETERS_ADDR 0x7A02000      // GPU parameters
#define GPU_PIXEL_DATA_ADDR 0x7B00000      // Pixel plane data

void gpu_clear_tables();
void gpu_clear_bg();
void gpu_clear_window();
void gpu_clear_pixel();
void gpu_clear_vram();
void gpu_load_pattern_table(const unsigned int *pattern_table);
void gpu_load_palette_table(const unsigned int *palette_table);
void gpu_set_bg_tile_scroll(unsigned int tile_x);
void gpu_set_bg_pixel_scroll(unsigned int pixel_x);
void gpu_set_window_palette(unsigned int palette_index);
void gpu_set_bg_palette(unsigned int palette_index);
void gpu_write_window_tile(unsigned int x, unsigned int y, unsigned int tile_index, unsigned int palette_index);
void gpu_write_bg_tile(unsigned int x, unsigned int y, unsigned int tile_index, unsigned int palette_index);
void gpu_write_pixel_data(unsigned int x, unsigned int y, unsigned int color);

#endif // GPU_HAL_H
