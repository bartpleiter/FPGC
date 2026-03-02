// mandelbrot_fp64.c - Mandelbrot rendered with FP64 coprocessor (32.32 fixed-point)
// Bare-metal program for FPGC/B32P3
// Demonstrates deep zoom capability beyond 16.16 fixed-point limits

#define COMMON_STDLIB
#include "libs/common/common.h"

#define KERNEL_GPU_HAL
#define KERNEL_TIMER
#include "libs/kernel/kernel.h"

// ---- Display parameters ----
#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240
#define PIXEL_FB_ADDR 0x7B00000

unsigned int *fb = (unsigned int *)PIXEL_FB_ADDR;

// ---- Colors: rrrgggbb (8-bit: 3R 3G 2B) ----
#define NUM_COLORS 16
unsigned int colors[NUM_COLORS] = {
  0x00, // 0: Black (in set)
  0x03, // 1: Dark blue
  0x07, // 2: Blue
  0x0B, // 3: Light blue
  0x0F, // 4: Cyan-ish
  0x2F, // 5: Cyan
  0x4F, // 6: Light cyan
  0x6F, // 7: Cyan-white
  0x9C, // 8: Green
  0xBC, // 9: Light green
  0xDC, // 10: Yellow-green
  0xFC, // 11: Yellow
  0xF8, // 12: Orange
  0xF0, // 13: Red-orange
  0xE0, // 14: Red
  0xA0  // 15: Dark red
};

// ---- FP register allocation ----
#define F_CRE  0  // c_re  (per-pixel, set in render loop)
#define F_CIM  1  // c_im  (per-row, set in render loop)
#define F_ZRE  2  // z_re  (iteration state)
#define F_ZIM  3  // z_im  (iteration state)
#define F_TMP1 4  // z_re^2, then |z|^2
#define F_TMP2 5  // z_im^2
#define F_TMP3 6  // z_re * z_im
#define F_STEP 7  // pixel step size (constant per frame)

// ---- Maximum iterations ----
#define MAX_ITER 110

// ---- Mandelbrot iteration for one pixel ----
// Assumes f0=c_re, f1=c_im are already loaded.
// Returns iteration count (0 = in set, 1..MAX_ITER = escaped)
int mandelbrot_pixel()
{
  // z = 0 + 0i
  __fld(F_ZRE, 0, 0);
  __fld(F_ZIM, 0, 0);

  int iter;
  for (iter = 0; iter < MAX_ITER; iter++)
  {
    // z_re^2 = z_re * z_re
    __fmul(F_TMP1, F_ZRE, F_ZRE);

    // z_im^2 = z_im * z_im
    __fmul(F_TMP2, F_ZIM, F_ZIM);

    // z_re * z_im (before we overwrite z_re)
    __fmul(F_TMP3, F_ZRE, F_ZIM);

    // z_re = z_re^2 - z_im^2 + c_re
    __fsub(F_ZRE, F_TMP1, F_TMP2);
    __fadd(F_ZRE, F_ZRE, F_CRE);

    // z_im = 2 * z_re * z_im + c_im
    __fadd(F_ZIM, F_TMP3, F_TMP3);   // 2 * z_re * z_im
    __fadd(F_ZIM, F_ZIM, F_CIM);

    // Escape check: |z|^2 = z_re^2 + z_im^2
    // f4 still holds z_re^2 from fmul, f5 still holds z_im^2
    // Wait - z_re was overwritten by fsub/fadd above, but f4/f5 hold
    // the ORIGINAL z_re^2 and z_im^2 before the update. 
    // Actually no - fmul writes to f4, then fsub(F_ZRE, F_TMP1, F_TMP2)
    // reads f4 and f5 but writes to f2 (F_ZRE). So f4 and f5 are still
    // the original z_re^2 and z_im^2. Good.
    __fadd(F_TMP1, F_TMP1, F_TMP2);  // |z|^2 = z_re^2 + z_im^2
    int mag_hi = __fsthi(F_TMP1);

    // |z|^2 > 4.0? In 32.32, 4.0 has hi=4
    // Use unsigned comparison: if mag_hi >= 5 (or > 4 signed)
    if (mag_hi > 4)
      return iter + 1;
    // Also catch mag_hi == 4 with nonzero fractional part
    if (mag_hi == 4 && __fstlo(F_TMP1) != 0)
      return iter + 1;
  }

  return 0; // In set
}

// ---- Render one frame ----
// center_re/im as {hi, lo} pairs, scale_hi/lo = view width
void render_mandelbrot(int center_re_hi, unsigned int center_re_lo,
                       int center_im_hi, unsigned int center_im_lo,
                       int scale_hi,     unsigned int scale_lo)
{
  // Compute step = scale / SCREEN_WIDTH
  // We do this in software since divfp isn't available for 64-bit.
  // step = scale / 320. Since 320 = 5 * 64, we can approximate:
  //   step = scale >> 8 - scale >> 10 + scale >> 13
  // But simpler: just do repeated subtraction / shift-based division.
  // Actually, for a power approach: step ~= scale * (1/320)
  // 1/320 in 32.32 = {0, 0x00D1745D} (0.003125 * 2^32 = 13421773)
  // Wait, 1/320 = 0.003125. 0.003125 * 2^32 = 13421772.8, so lo = 0x00CCCCCD
  // Let me recalculate: 1/320 = 1/320. 2^32/320 = 4294967296/320 = 13421772.8.
  // So 1/320 ≈ {0, 13421773} = {0, 0x00CCCCCD}
  //
  // For simplicity, compute step using the FP64 coprocessor:
  // Load scale into f7, load 320 into f6, then do f7 = scale
  // We can't do division in the coprocessor. Instead, multiply by reciprocal.
  // 1/320 in 32.32: hi=0, lo=13421773 (0x00CCCCCD)
  //
  // step = scale * (1/320). But we need fmul for that, and the result of
  // fmul({scale_hi, scale_lo} * {0, 0x00CCCCCD}) gives us the step.
  
  // Load the reciprocal of 320 into f6
  __fld(6, 0, 13421773);   // f6 = 1/320 in 32.32
  
  // Load scale into f7
  __fld(F_STEP, scale_hi, scale_lo);  // f7 = scale
  
  // step = scale * (1/320)
  __fmul(F_STEP, F_STEP, 6);  // f7 = step
  
  // Compute start_re = center_re - scale/2
  // start_im = center_im - step * (SCREEN_HEIGHT/2)
  // We can compute scale/2 by shifting: just subtract 1 from hi if lo < 0x80000000, 
  // or use fadd to halve... actually let's use the coprocessor:
  // Load 0.5 and multiply: half_scale = scale * 0.5
  // Or simply: half_scale.hi = scale_hi >> 1 with carry from bit 0
  // This is tricky with signed values. Let's use the FP coprocessor.
  
  // Load scale into f6, load 0.5 = {0, 0x80000000} into f5
  __fld(F_TMP2, 0, 0x80000000);   // f5 = 0.5
  __fld(6, scale_hi, scale_lo);   // f6 = scale
  __fmul(6, 6, F_TMP2);           // f6 = scale * 0.5 = half_width
  
  // start_re = center_re - half_width
  __fld(F_CRE, center_re_hi, center_re_lo);  // f0 = center_re
  __fsub(F_CRE, F_CRE, 6);                    // f0 = start_re
  
  // Now compute: half_height = step * (SCREEN_HEIGHT / 2) = step * 120
  // Load 120 into f6
  __fld(6, 120, 0);              // f6 = 120.0
  __fmul(6, F_STEP, 6);          // f6 = step * 120 = half_height
  
  // start_im = center_im - half_height
  __fld(F_CIM, center_im_hi, center_im_lo);  // f1 = center_im
  __fsub(F_CIM, F_CIM, 6);                    // f1 = start_im
  
  // Save start_re into CPU (we need to reset it each row)
  int start_re_hi = __fsthi(F_CRE);
  unsigned int start_re_lo = __fstlo(F_CRE);
  
  // Read step back to CPU (for incrementing)
  int step_hi = __fsthi(F_STEP);
  unsigned int step_lo = __fstlo(F_STEP);
  
  int pixel_index = 0;
  int y;
  int x;
  
  for (y = 0; y < SCREEN_HEIGHT; y++)
  {
    // Reset c_re to start_re for this row
    __fld(F_CRE, start_re_hi, start_re_lo);
    
    for (x = 0; x < SCREEN_WIDTH; x++)
    {
      int iter = mandelbrot_pixel();
      
      // Map iteration count to color
      unsigned int color;
      if (iter == 0)
      {
        color = colors[0]; // In set - black
      }
      else
      {
        color = colors[(iter % (NUM_COLORS - 1)) + 1];
      }
      
      fb[pixel_index] = color;
      pixel_index++;
      
      // c_re += step  (reload step into f7 since mandelbrot_pixel uses f4-f6)
      __fld(F_STEP, step_hi, step_lo);
      __fadd(F_CRE, F_CRE, F_STEP);
    }
    
    // c_im += step
    __fld(F_STEP, step_hi, step_lo);
    __fadd(F_CIM, F_CIM, F_STEP);
  }
}

// ---- 64-bit right shift by 1 (scale /= 2) ----
// For the zoom animation
void half_scale(int *hi, unsigned int *lo)
{
  unsigned int carry = (*hi & 1) ? 0x80000000 : 0;
  *hi = *hi >> 1;    // arithmetic shift (preserves sign)
  *lo = (*lo >> 1) | carry;
}

// ---- 64-bit multiply by a small fraction for lerp ----
// Computes (hi, lo) * frac where frac is {frac_hi, frac_lo}
// Result returned through the FP coprocessor
void fp64_mul(int a_hi, unsigned int a_lo, int b_hi, unsigned int b_lo,
              int *out_hi, unsigned int *out_lo)
{
  __fld(6, a_hi, a_lo);
  __fld(5, b_hi, b_lo);
  __fmul(6, 6, 5);
  *out_hi = __fsthi(6);
  *out_lo = __fstlo(6);
}

// ---- 64-bit addition ----
void fp64_add(int a_hi, unsigned int a_lo, int b_hi, unsigned int b_lo,
              int *out_hi, unsigned int *out_lo)
{
  __fld(6, a_hi, a_lo);
  __fld(5, b_hi, b_lo);
  __fadd(6, 6, 5);
  *out_hi = __fsthi(6);
  *out_lo = __fstlo(6);
}

// ---- 64-bit subtraction ----
void fp64_sub(int a_hi, unsigned int a_lo, int b_hi, unsigned int b_lo,
              int *out_hi, unsigned int *out_lo)
{
  __fld(6, a_hi, a_lo);
  __fld(5, b_hi, b_lo);
  __fsub(6, 6, 5);
  *out_hi = __fsthi(6);
  *out_lo = __fstlo(6);
}

void init()
{
  gpu_clear_vram();
  timer_init();
}

int main()
{
  init();
  
  // Initial view: full Mandelbrot set
  // center = (-0.5, 0.0), width = 3.5
  // -0.5 in 32.32: {-1, 0x80000000}  (because -1 + 0.5 = -0.5)
  int center_re_hi = -1;
  unsigned int center_re_lo = 0x80000000;
  int center_im_hi = 0;
  unsigned int center_im_lo = 0;
  
  // scale (view width) = 3.5 in 32.32: {3, 0x80000000}
  int scale_hi = 3;
  unsigned int scale_lo = 0x80000000;
  
  // Zoom target: "seahorse valley" at approximately
  // Real: -0.743643887037151 → {-1, 0x41585C30} (approx)
  // Imag:  0.131825904205330 → {0, 0x21C7A350}  (approx)
  // -0.743643887037151 = -1 + 0.256356112962849
  //   0.256356112962849 * 2^32 = 1101133872 = 0x41A1BEB0
  int target_re_hi = -1;
  unsigned int target_re_lo = 0x41A1BEB0;
  int target_im_hi = 0;
  // 0.131825904205330 * 2^32 = 566166544 = 0x21C01C90
  unsigned int target_im_lo = 0x21C01C90;
  
  // Minimum scale before we restart (much smaller than 16.16 version!)
  // 16.16 limit was ~0.004 (scale=256). With 32.32, we can go to ~1e-9.
  // min_scale = 0.000001 ≈ {0, 0x000010C7} 
  // Let's use a reasonable zoom depth: {0, 0x00001000} ≈ 9.5e-7
  int min_scale_hi = 0;
  unsigned int min_scale_lo = 0x00001000;
  
  while (1)
  {
    // Render current view
    render_mandelbrot(center_re_hi, center_re_lo,
                      center_im_hi, center_im_lo,
                      scale_hi, scale_lo);
    
    // Brief pause to see the frame
    delay(500);
    
    // Move center towards target using lerp
    // diff = target - center
    int diff_re_hi;
    unsigned int diff_re_lo;
    int diff_im_hi;
    unsigned int diff_im_lo;
    
    fp64_sub(target_re_hi, target_re_lo, center_re_hi, center_re_lo,
             &diff_re_hi, &diff_re_lo);
    fp64_sub(target_im_hi, target_im_lo, center_im_hi, center_im_lo,
             &diff_im_hi, &diff_im_lo);
    
    // center += diff (full speed lerp, same as original)
    fp64_add(center_re_hi, center_re_lo, diff_re_hi, diff_re_lo,
             &center_re_hi, &center_re_lo);
    fp64_add(center_im_hi, center_im_lo, diff_im_hi, diff_im_lo,
             &center_im_hi, &center_im_lo);
    
    // Zoom in: scale *= 0.5
    half_scale(&scale_hi, &scale_lo);
    
    // Check if we've hit zoom limit
    if (scale_hi == 0 && scale_lo < min_scale_lo)
    {
      // Reset to initial view
      center_re_hi = -1;
      center_re_lo = 0x80000000;
      center_im_hi = 0;
      center_im_lo = 0;
      scale_hi = 3;
      scale_lo = 0x80000000;
      
      delay(5000);
    }
  }
  
  return 0;
}

void interrupt()
{
  int int_id = get_int_id();
  switch (int_id)
  {
    case INTID_TIMER2:
      timer_isr_handler(TIMER_2);
      break;
    default:
      break;
  }
}
