#ifndef FPGC_GPU_HAL_H
#define FPGC_GPU_HAL_H

/* Clear all GPU VRAM regions */
void gpu_clear_vram(void);
void gpu_clear_tables(void);
void gpu_clear_bg(void);
void gpu_clear_window(void);
void gpu_clear_pixel(void);

/* Load pattern (1024 words) and palette (32 words) tables */
void gpu_load_pattern_table(const unsigned int *pattern_table);
void gpu_load_palette_table(const unsigned int *palette_table);

/* Background plane scroll */
void gpu_set_bg_tile_scroll(unsigned int tile_x);
void gpu_set_bg_pixel_scroll(unsigned int pixel_x);

/* Set palette for entire plane */
void gpu_set_window_palette(unsigned int palette_index);
void gpu_set_bg_palette(unsigned int palette_index);

/* Write individual tiles */
void gpu_write_window_tile(unsigned int x, unsigned int y,
                           unsigned int tile_index, unsigned int palette_index);
void gpu_write_bg_tile(unsigned int x, unsigned int y,
                       unsigned int tile_index, unsigned int palette_index);

/* Pixel plane */
void gpu_write_pixel_data(unsigned int x, unsigned int y, unsigned int color);
void gpu_set_pixel_palette(unsigned int index, unsigned int rgb24);
unsigned int gpu_get_pixel_palette(unsigned int index);
void gpu_reset_pixel_palette(void);

#endif /* FPGC_GPU_HAL_H */
