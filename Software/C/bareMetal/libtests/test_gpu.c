// Test program for GPU graphics library
#define KERNEL_GPU_FB
#define KERNEL_GPU_DATA_ASCII
#include "libs/kernel/kernel.h"

unsigned int color = 0;
unsigned int framecounter = 0;
unsigned int render = 1;

void render_loop()
{
    gpu_write_pixel_data(160, 120, color);
    gpu_write_pixel_data(160, 121, color);
    gpu_write_pixel_data(161, 120, color);
    gpu_write_pixel_data(161, 121, color);

    fb_draw_circle(160, 120, 50, color + 0xF0);

    fb_fill_rect(10, 10, 50, 50, color + 0xAA);

    fb_draw_line(100, 100, 20, 20, color + 0x03);

    fb_draw_rect(150, 150, 40, 30, color + 0x0F);
}

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

    
    while (1)
    {
        if (render)
        {
            render = 0;
            render_loop();
        }
    }

    return 1;
}



void interrupt()
{
    render = 1;
    framecounter++;
    color = framecounter >> 2;
}
