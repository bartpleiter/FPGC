// Test program for GPU graphics library
#define KERNEL_GPU_FB
#define KERNEL_GPU_DATA_ASCII
#include "libs/kernel/kernel.h"

int main() {
    // Reset GPU VRAM
    gpu_clear_vram();

    // Load default pattern and palette tables
    unsigned int* pattern_table = (unsigned int*)&DATA_ASCII_DEFAULT;
    gpu_load_pattern_table(pattern_table + 3); // +3 to skip function prologue

    unsigned int* palette_table = (unsigned int*)&DATA_PALETTE_DEFAULT;
    gpu_load_palette_table(palette_table + 3); // +3 to skip function prologue

    // Do some test writes to window and background planes
    gpu_write_window_tile(0, 0, 65, 8);
    gpu_write_window_tile(1, 0, 66, 8);
    gpu_write_window_tile(0, 1, 67, 8);
    gpu_write_window_tile(1, 1, 68, 8);

    gpu_write_bg_tile(10, 10, 65, 5);
    gpu_write_bg_tile(11, 10, 66, 5);
    gpu_write_bg_tile(10, 11, 67, 5);
    gpu_write_bg_tile(11, 11, 68, 5);

    gpu_write_pixel_data(160, 120, 0xFF);
    gpu_write_pixel_data(160, 121, 0xFF);
    gpu_write_pixel_data(161, 120, 0xFF);
    gpu_write_pixel_data(161, 121, 0xFF);

    fb_draw_circle(160, 120, 50, 0xF0);

    fb_fill_rect(10, 10, 50, 50, 0xAA);

    fb_draw_line(100, 100, 20, 20, 0x03);

    fb_draw_rect(150, 150, 40, 30, 0x0F);
    
    
    return 1;
}

void interrupt()
{
    // No need to handle interrupts
}
