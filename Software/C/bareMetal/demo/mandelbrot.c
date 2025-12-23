#define COMMON_STDLIB
#define COMMON_FIXEDMATH
#include "libs/common/common.h"

#define KERNEL_GPU_FB
#define KERNEL_TIMER
#include "libs/kernel/kernel.h"

// Colors are rrrgggbb format (8-bit: 3 bits red, 3 bits green, 2 bits blue)
// Color palette for Mandelbrot iterations
#define NUM_COLORS 16
unsigned int colors[NUM_COLORS] = {
  0x00, // Black (in set)
  0x03, // Dark blue
  0x07, // Blue
  0x0B, // Light blue
  0x0F, // Cyan-ish
  0x2F, // Cyan
  0x4F, // Light cyan
  0x6F, // Cyan-white
  0x9C, // Green
  0xBC, // Light green
  0xDC, // Yellow-green
  0xFC, // Yellow
  0xF8, // Orange
  0xF0, // Red-orange
  0xE0, // Red
  0xA0  // Dark red
};

// Maximum iterations for escape test
// Lower = faster but less detail
#define MAX_ITER 86

// Interesting point to zoom into (near the "seahorse valley")
// These are in 16.16 fixed point format
// Real: -0.743643887037151 (approximately -48734 in 16.16)
// Imag:  0.131825904205330 (approximately 8640 in 16.16)
#define ZOOM_TARGET_RE  -48734
#define ZOOM_TARGET_IM  8640

// Initial view parameters (full Mandelbrot set)
// Real range: -2.5 to 1.0, Imag range: -1.5 to 1.5 (adjusted for aspect ratio)
#define INITIAL_CENTER_RE  -32768   // -0.5 in 16.16
#define INITIAL_CENTER_IM  0        // 0.0 in 16.16
#define INITIAL_SCALE      196608   // 3.0 in 16.16 (width of view)

// Direct memory access to pixel framebuffer for faster rendering
#define PIXEL_FB_ADDR 0x7B00000

// Render at half resolution and double the pixels for speed
#define RENDER_WIDTH  320
#define RENDER_HEIGHT 240

int current_frame = 0;

unsigned int *fb = (unsigned int *)PIXEL_FB_ADDR;

void init()
{
  // Reset GPU VRAM (clear pixel plane)
  gpu_clear_vram();
  
  // Initialize timer for delay
  timer_init();
}


// Compute one pixel of the Mandelbrot set
// Returns iteration count (0 = in set, 1-MAX_ITER = escaped)
// Optimized version with early bailout
int mandelbrot_pixel(fixed_t c_re, fixed_t c_im)
{
  fixed_t z_re = 0;
  fixed_t z_im = 0;
  int iter;
  
  // Escape radius squared in 16.16: 4.0 = 262144
  fixed_t escape_radius_sq = 262144;
  
  // Quick cardioid/bulb check to skip expensive iterations
  // Main cardioid: (x - 0.25)^2 + y^2 < (0.5 - 0.5*cos(theta))^2
  // Period-2 bulb: (x + 1)^2 + y^2 < 0.0625
  fixed_t c_re_plus_1 = c_re + FIXED_ONE;  // c_re + 1
  fixed_t q = __multfp(c_re - 16384, c_re - 16384) + __multfp(c_im, c_im);  // (c_re - 0.25)^2 + c_im^2
  
  // Check period-2 bulb
  if (__multfp(c_re_plus_1, c_re_plus_1) + __multfp(c_im, c_im) < 4096)  // 0.0625 in 16.16
  {
    return 0; // In set
  }
  
  // Simplified cardioid check
  fixed_t p = __multfp(c_re - 16384, c_re - 16384) + __multfp(c_im, c_im);
  if (__multfp(p, p + (c_re - 16384)) < __multfp(c_im, c_im) >> 2)
  {
    return 0; // In set (approximately)
  }
  
  for (iter = 0; iter < MAX_ITER; iter++)
  {
    // z_re^2 and z_im^2
    fixed_t z_re_sq = __multfp(z_re, z_re);
    fixed_t z_im_sq = __multfp(z_im, z_im);
    
    // Check escape condition: |z|^2 > 4
    if (z_re_sq + z_im_sq > escape_radius_sq)
    {
      return iter + 1;
    }
    
    // z = z^2 + c
    // z_im = 2 * z_re * z_im + c_im
    fixed_t new_z_im = __multfp(z_re, z_im);
    new_z_im = new_z_im + new_z_im + c_im;  // 2 * z_re * z_im + c_im
    
    // z_re = z_re^2 - z_im^2 + c_re
    z_re = z_re_sq - z_im_sq + c_re;
    z_im = new_z_im;
  }
  
  return 0; // In set
}

// Render the Mandelbrot set with given center and scale
void render_mandelbrot(fixed_t center_re, fixed_t center_im, fixed_t scale)
{
  int x, y;
  int pixel_index;
  
  // Calculate step sizes for reduced resolution
  // scale is the width of the view in fixed-point
  fixed_t step = __divfp(scale, int2fixed(RENDER_WIDTH));
  
  // Calculate top-left corner of view  
  fixed_t half_scale = scale >> 1;
  fixed_t start_re = center_re - half_scale;
  fixed_t start_im = center_im - __multfp(step, int2fixed(RENDER_HEIGHT >> 1));
  
  pixel_index = 0;
  fixed_t c_im = start_im;
  
  for (y = 0; y < RENDER_HEIGHT; y++)
  {
    fixed_t c_re = start_re;
    
    for (x = 0; x < RENDER_WIDTH; x++)
    {
      int iter = mandelbrot_pixel(c_re, c_im);
      
      // Map iteration count to color
      unsigned int color;
      if (iter == 0)
      {
        color = colors[0]; // In set - black
      }
      else
      {
        // Map iteration count to color palette with cycling
        color = colors[(iter % (NUM_COLORS - 1)) + 1];
      }
      
      fb[pixel_index] = color;
      pixel_index++;
      
      c_re += step;
    }
    
    c_im += step;
  }
}

int main()
{
  init();
    
  // Current view parameters
  fixed_t center_re = INITIAL_CENTER_RE;
  fixed_t center_im = INITIAL_CENTER_IM;
  fixed_t scale = INITIAL_SCALE;
  
  // Zoom factor per frame (0.75 in 16.16 = 49152)
  // Faster zoom for demo effect
  fixed_t zoom_factor = 49152;
  
  // Minimum scale before we hit precision limits
  // At around scale = 256 (0.004), we start seeing pixelation due to 16.16 limits
  fixed_t min_scale = 512;  // About 0.008 in real units
  
  // Interpolation speed for center movement (0.9 in 16.16 = 58982)
  fixed_t lerp_speed = FIXED_ONE;
  
  while (1)
  {
    // Render current view
    render_mandelbrot(center_re, center_im, scale);
    
    // Move center towards target
    fixed_t diff_re = ZOOM_TARGET_RE - center_re;
    fixed_t diff_im = ZOOM_TARGET_IM - center_im;
    center_re += __multfp(diff_re, lerp_speed);
    center_im += __multfp(diff_im, lerp_speed);
    
    // Zoom in
    scale = __multfp(scale, zoom_factor);
    
    // Check if we've hit precision limits
    if (scale < min_scale)
    {
      
      // Reset to initial view
      center_re = INITIAL_CENTER_RE;
      center_im = INITIAL_CENTER_IM;
      scale = INITIAL_SCALE;
      current_frame = 0;
      
      // Small delay before restart
      delay(5000);
    }
    
    current_frame++;
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
