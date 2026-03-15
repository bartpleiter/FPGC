/*
 * gpu_hal.c — GPU Hardware Abstraction Layer for B32P3/FPGC.
 *
 * Provides VRAM access through plain pointer casts (not volatile).
 * VRAM writes are simple memory stores — no special I/O protocol needed.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "fpgc.h"
#include "gpu_hal.h"

void
gpu_clear_tables(void)
{
    int i;
    unsigned int *p;

    p = (unsigned int *)FPGC_GPU_PATTERN_TABLE;
    for (i = 0; i < 1024; i++)
        p[i] = 0;

    p = (unsigned int *)FPGC_GPU_PALETTE_TABLE;
    for (i = 0; i < 32; i++)
        p[i] = 0;
}

void
gpu_clear_bg(void)
{
    int i;
    unsigned int *p;

    p = (unsigned int *)FPGC_GPU_BG_TILE_TABLE;
    for (i = 0; i < (64 * 25); i++)
        p[i] = 0;

    p = (unsigned int *)FPGC_GPU_BG_COLOR_TABLE;
    for (i = 0; i < (64 * 25); i++)
        p[i] = 0;

    p = (unsigned int *)FPGC_GPU_PARAMS;
    p[0] = 0;
    p[1] = 0;
}

void
gpu_clear_window(void)
{
    int i;
    unsigned int *p;

    p = (unsigned int *)FPGC_GPU_WIN_TILE_TABLE;
    for (i = 0; i < (40 * 25); i++)
        p[i] = 0;

    p = (unsigned int *)FPGC_GPU_WIN_COLOR_TABLE;
    for (i = 0; i < (40 * 25); i++)
        p[i] = 0;
}

void
gpu_clear_pixel(void)
{
    int i;
    unsigned int *p = (unsigned int *)FPGC_GPU_PIXEL_DATA;
    for (i = 0; i < (320 * 240); i++)
        p[i] = 0;
}

void
gpu_clear_vram(void)
{
    gpu_clear_tables();
    gpu_clear_bg();
    gpu_clear_window();
    gpu_clear_pixel();
}

void
gpu_load_pattern_table(const unsigned int *pattern_table)
{
    int i;
    unsigned int *p = (unsigned int *)FPGC_GPU_PATTERN_TABLE;
    for (i = 0; i < 1024; i++)
        p[i] = pattern_table[i];
}

void
gpu_load_palette_table(const unsigned int *palette_table)
{
    int i;
    unsigned int *p = (unsigned int *)FPGC_GPU_PALETTE_TABLE;
    for (i = 0; i < 32; i++)
        p[i] = palette_table[i];
}

void
gpu_set_bg_tile_scroll(unsigned int tile_x)
{
    unsigned int *p = (unsigned int *)FPGC_GPU_PARAMS;
    p[0] = tile_x;
}

void
gpu_set_bg_pixel_scroll(unsigned int pixel_x)
{
    unsigned int *p = (unsigned int *)FPGC_GPU_PARAMS;
    p[1] = pixel_x;
}

void
gpu_set_window_palette(unsigned int palette_index)
{
    int i;
    unsigned int *p = (unsigned int *)FPGC_GPU_WIN_COLOR_TABLE;
    for (i = 0; i < (40 * 25); i++)
        p[i] = palette_index;
}

void
gpu_set_bg_palette(unsigned int palette_index)
{
    int i;
    unsigned int *p = (unsigned int *)FPGC_GPU_BG_COLOR_TABLE;
    for (i = 0; i < (64 * 25); i++)
        p[i] = palette_index;
}

void
gpu_write_window_tile(unsigned int x, unsigned int y,
                      unsigned int tile_index, unsigned int palette_index)
{
    unsigned int *tile_p  = (unsigned int *)FPGC_GPU_WIN_TILE_TABLE;
    unsigned int *color_p = (unsigned int *)FPGC_GPU_WIN_COLOR_TABLE;
    unsigned int idx;

    if (x >= 40) x = x % 40;
    if (y >= 25) y = y % 25;
    idx = y * 40 + x;

    tile_p[idx]  = tile_index;
    color_p[idx] = palette_index;
}

void
gpu_write_bg_tile(unsigned int x, unsigned int y,
                  unsigned int tile_index, unsigned int palette_index)
{
    unsigned int *tile_p  = (unsigned int *)FPGC_GPU_BG_TILE_TABLE;
    unsigned int *color_p = (unsigned int *)FPGC_GPU_BG_COLOR_TABLE;
    unsigned int idx;

    if (x >= 64) x = x % 64;
    if (y >= 25) y = y % 25;
    idx = y * 64 + x;

    tile_p[idx]  = tile_index;
    color_p[idx] = palette_index;
}

void
gpu_write_pixel_data(unsigned int x, unsigned int y, unsigned int color)
{
    unsigned int *p = (unsigned int *)FPGC_GPU_PIXEL_DATA;
    unsigned int idx;

    if (x >= 320) x = x % 320;
    if (y >= 240) y = y % 240;
    idx = y * 320 + x;

    p[idx] = color;
}

void
gpu_set_pixel_palette(unsigned int index, unsigned int rgb24)
{
    unsigned int *p = (unsigned int *)(FPGC_GPU_PIXEL_PALETTE + index * 4);
    *p = rgb24;
}

void
gpu_reset_pixel_palette(void)
{
    int i;
    for (i = 0; i < 256; i++) {
        int r3 = (i >> 5) & 7;
        int g3 = (i >> 2) & 7;
        int b2 = i & 3;

        int r = (r3 << 5) | (r3 << 2) | (r3 >> 1);
        int g = (g3 << 5) | (g3 << 2) | (g3 >> 1);
        int b = (b2 << 6) | (b2 << 4) | (b2 << 2) | b2;

        gpu_set_pixel_palette(i, (unsigned int)((r << 16) | (g << 8) | b));
    }
}
