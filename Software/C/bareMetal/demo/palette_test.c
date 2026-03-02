// palette_test.c - GPU Programmable Pixel Palette test
// Bare-metal program for FPGC/B32P3
//
// Draws 256 vertical color bars using the pixel framebuffer,
// then programs the palette to show smooth color gradients.
//
// Test stages:
//   1. Fill screen with default palette (RRRGGGBB mapping) — should look normal
//   2. After short delay, set a custom gradient palette — colors should change
//   3. Cycle palette continuously for visual validation

#define COMMON_STDLIB
#include "libs/common/common.h"

#define KERNEL_GPU_HAL
#define KERNEL_TIMER
#include "libs/kernel/kernel.h"

// ---- Display parameters ----
#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240
#define PIXEL_FB_ADDR 0x7B00000

// ---- Pixel Palette address ----
#define PALETTE_ADDR  0x7B20000

// Write one 24-bit RGB palette entry
void set_palette(int index, int rgb24)
{
    *(volatile int*)(PALETTE_ADDR + index) = rgb24;
}

// ---- HSV to RGB (fixed-point, all integer math) ----
// h: 0-255 (hue), s: 0-255 (saturation), v: 0-255 (value)
// Returns 0x00RRGGBB
int hsv_to_rgb(int h, int s, int v)
{
    int region, remainder, p, q, t;
    int r, g, b;

    if (s == 0)
    {
        return (v << 16) | (v << 8) | v;
    }

    region = h / 43;
    remainder = (h - (region * 43)) * 6;

    p = (v * (255 - s)) >> 8;
    q = (v * (255 - ((s * remainder) >> 8))) >> 8;
    t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;

    if (region == 0)      { r = v; g = t; b = p; }
    else if (region == 1) { r = q; g = v; b = p; }
    else if (region == 2) { r = p; g = v; b = t; }
    else if (region == 3) { r = p; g = q; b = v; }
    else if (region == 4) { r = t; g = p; b = v; }
    else                  { r = v; g = p; b = q; }

    return (r << 16) | (g << 8) | b;
}

// ---- Draw 256 color bars ----
// Each column i (0-319) uses pixel value (i * 256 / 320)
// This fills the screen so every palette entry is visible
void draw_color_bars()
{
    volatile unsigned int *fb = (volatile unsigned int *)PIXEL_FB_ADDR;
    int x, y;

    for (y = 0; y < SCREEN_HEIGHT; y++)
    {
        for (x = 0; x < SCREEN_WIDTH; x++)
        {
            // Map x (0-319) to palette index (0-255)
            int index = (x * 256) / SCREEN_WIDTH;
            fb[y * SCREEN_WIDTH + x] = index;
        }
    }
}

// ---- Palette presets ----

// Smooth HSV rainbow gradient (full saturation, full brightness)
void load_rainbow_palette()
{
    int i;
    set_palette(0, 0x000000); // Index 0 = black
    for (i = 1; i < 256; i++)
    {
        set_palette(i, hsv_to_rgb(i, 255, 255));
    }
}

// Fire palette: black → dark red → orange → yellow → white
void load_fire_palette()
{
    int i;
    set_palette(0, 0x000000);
    for (i = 1; i < 256; i++)
    {
        int r, g, b;
        if (i < 85)
        {
            r = i * 3;
            g = 0;
            b = 0;
        }
        else if (i < 170)
        {
            r = 255;
            g = (i - 85) * 3;
            b = 0;
        }
        else
        {
            r = 255;
            g = 255;
            b = (i - 170) * 3;
        }
        set_palette(i, (r << 16) | (g << 8) | b);
    }
}

// Ice palette: black → deep blue → cyan → white
void load_ice_palette()
{
    int i;
    set_palette(0, 0x000000);
    for (i = 1; i < 256; i++)
    {
        int r, g, b;
        if (i < 85)
        {
            r = 0;
            g = 0;
            b = i * 3;
        }
        else if (i < 170)
        {
            r = 0;
            g = (i - 85) * 3;
            b = 255;
        }
        else
        {
            r = (i - 170) * 3;
            g = 255;
            b = 255;
        }
        set_palette(i, (r << 16) | (g << 8) | b);
    }
}

// Restore default RRRGGGBB bit-replication palette
void load_default_palette()
{
    int i;
    for (i = 0; i < 256; i++)
    {
        int r3 = (i >> 5) & 7;
        int g3 = (i >> 2) & 7;
        int b2 = i & 3;

        // Replicate bits to fill 8-bit channels
        int r = (r3 << 5) | (r3 << 2) | (r3 >> 1);
        int g = (g3 << 5) | (g3 << 2) | (g3 >> 1);
        int b = (b2 << 6) | (b2 << 4) | (b2 << 2) | b2;

        set_palette(i, (r << 16) | (g << 8) | b);
    }
}

// Palette rotation: shift all entries by 1 position (except index 0)
void rotate_palette(int *buffer)
{
    int i;
    int saved = buffer[255];
    for (i = 255; i > 1; i--)
    {
        buffer[i] = buffer[i - 1];
    }
    buffer[1] = saved;

    // Write rotated palette to hardware
    for (i = 0; i < 256; i++)
    {
        set_palette(i, buffer[i]);
    }
}

int main()
{
    int palette_buf[256];
    int palette_index;
    int frame;
    int i;

    // Clear screen first
    gpu_clear_pixel();

    // Stage 1: Draw color bars with default palette
    // The default palette is initialized in hardware (RRRGGGBB bit-replication)
    draw_color_bars();

    // Wait 2 seconds to show default palette
    delay(2000);

    // Stage 2: Load rainbow palette — colors should visibly change
    load_rainbow_palette();

    // Copy current palette to buffer for rotation
    for (i = 0; i < 256; i++)
    {
        palette_buf[i] = hsv_to_rgb(i, 255, 255);
    }
    palette_buf[0] = 0x000000;

    // Wait 2 seconds to appreciate the rainbow
    delay(2000);

    // Stage 3: Cycle through palette presets
    palette_index = 0;
    frame = 0;

    while (1)
    {
        // Rotate palette for animation
        rotate_palette(palette_buf);

        delay(50); // ~20 FPS animation
        frame++;

        // Switch palette every 200 frames (~10 seconds)
        if (frame >= 200)
        {
            frame = 0;
            palette_index = (palette_index + 1) % 4;

            if (palette_index == 0)
            {
                load_rainbow_palette();
                for (i = 1; i < 256; i++)
                    palette_buf[i] = hsv_to_rgb(i, 255, 255);
                palette_buf[0] = 0x000000;
            }
            else if (palette_index == 1)
            {
                load_fire_palette();
                for (i = 1; i < 256; i++)
                {
                    int r, g, b;
                    if (i < 85) { r = i * 3; g = 0; b = 0; }
                    else if (i < 170) { r = 255; g = (i - 85) * 3; b = 0; }
                    else { r = 255; g = 255; b = (i - 170) * 3; }
                    palette_buf[i] = (r << 16) | (g << 8) | b;
                }
                palette_buf[0] = 0x000000;
            }
            else if (palette_index == 2)
            {
                load_ice_palette();
                for (i = 1; i < 256; i++)
                {
                    int r, g, b;
                    if (i < 85) { r = 0; g = 0; b = i * 3; }
                    else if (i < 170) { r = 0; g = (i - 85) * 3; b = 255; }
                    else { r = (i - 170) * 3; g = 255; b = 255; }
                    palette_buf[i] = (r << 16) | (g << 8) | b;
                }
                palette_buf[0] = 0x000000;
            }
            else
            {
                load_default_palette();
                for (i = 0; i < 256; i++)
                {
                    int r3 = (i >> 5) & 7;
                    int g3 = (i >> 2) & 7;
                    int b2 = i & 3;
                    int r = (r3 << 5) | (r3 << 2) | (r3 >> 1);
                    int g = (g3 << 5) | (g3 << 2) | (g3 >> 1);
                    int b = (b2 << 6) | (b2 << 4) | (b2 << 2) | b2;
                    palette_buf[i] = (r << 16) | (g << 8) | b;
                }
            }
        }
    }

    return 0;
}

void interrupt()
{
  int int_id = get_int_id();
  switch (int_id)
  {
    case INTID_TIMER2: // Needed for delay()
      timer_isr_handler(TIMER_2);
      break;
    default:
      break;
  }
}
