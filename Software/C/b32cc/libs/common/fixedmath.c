//
// fixedmath library implementation.
//

#include "libs/common/fixedmath.h"

// Sine lookup table (0-90 degrees, in fixed-point).
// Values are sin(angle) * FRACUNIT.
// The table covers 0-90 degrees in 1-degree increments.
// Other quadrants are computed by symmetry.
static const fixed_t sin_table[91] = {
  0,     // 0
  1143,  // 1
  2287,  // 2
  3429,  // 3
  4571,  // 4
  5711,  // 5
  6850,  // 6
  7986,  // 7
  9120,  // 8
  10252, // 9
  11380, // 10
  12504, // 11
  13625, // 12
  14742, // 13
  15854, // 14
  16961, // 15
  18064, // 16
  19160, // 17
  20251, // 18
  21336, // 19
  22414, // 20
  23486, // 21
  24550, // 22
  25606, // 23
  26655, // 24
  27696, // 25
  28729, // 26
  29752, // 27
  30767, // 28
  31772, // 29
  32768, // 30
  33753, // 31
  34728, // 32
  35693, // 33
  36647, // 34
  37589, // 35
  38521, // 36
  39440, // 37
  40347, // 38
  41243, // 39
  42125, // 40
  42995, // 41
  43852, // 42
  44695, // 43
  45525, // 44
  46340, // 45
  47142, // 46
  47929, // 47
  48702, // 48
  49460, // 49
  50203, // 50
  50931, // 51
  51643, // 52
  52339, // 53
  53019, // 54
  53683, // 55
  54331, // 56
  54963, // 57
  55577, // 58
  56175, // 59
  56755, // 60
  57319, // 61
  57864, // 62
  58393, // 63
  58903, // 64
  59395, // 65
  59870, // 66
  60326, // 67
  60763, // 68
  61183, // 69
  61583, // 70
  61965, // 71
  62328, // 72
  62672, // 73
  62997, // 74
  63302, // 75
  63589, // 76
  63856, // 77
  64103, // 78
  64331, // 79
  64540, // 80
  64729, // 81
  64898, // 82
  65047, // 83
  65176, // 84
  65286, // 85
  65376, // 86
  65446, // 87
  65496, // 88
  65526, // 89
  65536  // 90 = FRACUNIT
};

// Arctangent lookup table for atan2.
// Maps atan(y/x) * 256 / 45 for y/x from 0 to 1.
// Index is (y/x) * 256, value is angle * 256 / 45.
static const unsigned char atan_table[257] = {
  0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7,
  8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13, 13, 14, 14, 15,
  15, 16, 16, 17, 17, 17, 18, 18, 19, 19, 20, 20, 20, 21, 21, 22,
  22, 22, 23, 23, 24, 24, 24, 25, 25, 26, 26, 26, 27, 27, 27, 28,
  28, 29, 29, 29, 30, 30, 30, 31, 31, 31, 32, 32, 32, 33, 33, 33,
  34, 34, 34, 35, 35, 35, 36, 36, 36, 36, 37, 37, 37, 38, 38, 38,
  38, 39, 39, 39, 40, 40, 40, 40, 41, 41, 41, 41, 42, 42, 42, 42,
  43, 43, 43, 43, 44, 44, 44, 44, 44, 45, 45, 45, 45, 46, 46, 46,
  46, 46, 47, 47, 47, 47, 47, 48, 48, 48, 48, 48, 49, 49, 49, 49,
  49, 50, 50, 50, 50, 50, 50, 51, 51, 51, 51, 51, 51, 52, 52, 52,
  52, 52, 52, 53, 53, 53, 53, 53, 53, 53, 54, 54, 54, 54, 54, 54,
  54, 55, 55, 55, 55, 55, 55, 55, 55, 56, 56, 56, 56, 56, 56, 56,
  56, 57, 57, 57, 57, 57, 57, 57, 57, 57, 58, 58, 58, 58, 58, 58,
  58, 58, 58, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 60, 60, 60,
  60, 60, 60, 60, 60, 60, 60, 60, 61, 61, 61, 61, 61, 61, 61, 61,
  61, 61, 61, 61, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62,
  64 // 45 degrees at index 256
};

// ---- Conversion Functions ----

// Convert integer to fixed-point representation.
fixed_t int2fixed(int x)
{
  return (fixed_t)(x << FRACBITS);
}

// Convert fixed-point value to integer by truncation.
int fixed2int(fixed_t x)
{
  return x >> FRACBITS;
}

// Return the fractional part of a fixed-point value.
fixed_t fixed_frac(fixed_t x)
{
  return x & FRACMASK;
}

// ---- Basic Arithmetic Functions ----

// Compute square root of a fixed-point value using Newton-Raphson.
fixed_t fixed_sqrt(fixed_t x)
{
  fixed_t guess, prev_guess;
  int i;

  if (x <= 0)
  {
    return 0;
  }

  if (x > FRACUNIT)
  {
    guess = x >> 1;
  }
  else
  {
    guess = FRACUNIT;
  }

  for (i = 0; i < 16; i++)
  {
    prev_guess = guess;
    guess = (guess + __divfp(x, guess)) >> 1;

    if (guess == prev_guess)
    {
      break;
    }
  }

  return guess;
}

// ---- Trigonometric Functions ----

// Compute sine of an angle in degrees.
fixed_t fixed_sin(int angle)
{
  int quadrant;
  int idx;
  fixed_t result;

  angle = angle % 360;
  if (angle < 0)
  {
    angle += 360;
  }

  quadrant = angle / 90;
  idx = angle % 90;

  switch (quadrant)
  {
  case 0:
    result = sin_table[idx];
    break;
  case 1:
    result = sin_table[90 - idx];
    break;
  case 2:
    result = -sin_table[idx];
    break;
  case 3:
    result = -sin_table[90 - idx];
    break;
  default:
    result = 0;
    break;
  }

  return result;
}

// Compute cosine of an angle in degrees.
fixed_t fixed_cos(int angle)
{
  return fixed_sin(angle + 90);
}

// Compute tangent of an angle in degrees.
fixed_t fixed_tan(int angle)
{
  fixed_t s, c;

  s = fixed_sin(angle);
  c = fixed_cos(angle);

  if (c == 0)
  {
    return (s >= 0) ? 0x7FFFFFFF : -0x7FFFFFFF;
  }

  return __divfp(s, c);
}

// Compute angle in degrees from x/y coordinates.
int fixed_atan2(fixed_t y, fixed_t x)
{
  int angle;
  fixed_t abs_x, abs_y;
  int octant;
  unsigned int idx;
  fixed_t ratio;

  if (x == 0)
  {
    if (y > 0)
      return 90;
    if (y < 0)
      return 270;
    return 0;
  }

  if (y == 0)
  {
    return (x > 0) ? 0 : 180;
  }

  abs_x = (x < 0) ? -x : x;
  abs_y = (y < 0) ? -y : y;

  octant = 0;
  if (y < 0)
    octant |= 4;
  if (x < 0)
    octant |= 2;
  if (abs_y > abs_x)
    octant |= 1;

  if (abs_y > abs_x)
  {
    ratio = __divfp(abs_x << 8, abs_y); // Scale to 0-256 range
  }
  else
  {
    ratio = __divfp(abs_y << 8, abs_x);
  }

  idx = (unsigned int)fixed2int(ratio);
  if (idx > 256)
  {
    idx = 256;
  }

  angle = (atan_table[idx] * 45) >> 6;

  switch (octant)
  {
  case 0: // +x, +y, |x| >= |y|
    break;
  case 1: // +x, +y, |y| > |x|
    angle = 90 - angle;
    break;
  case 2: // -x, +y, |x| >= |y|
    angle = 180 - angle;
    break;
  case 3: // -x, +y, |y| > |x|
    angle = 90 + angle;
    break;
  case 4: // +x, -y, |x| >= |y|
    angle = 360 - angle;
    break;
  case 5: // +x, -y, |y| > |x|
    angle = 270 + angle;
    break;
  case 6: // -x, -y, |x| >= |y|
    angle = 180 + angle;
    break;
  case 7: // -x, -y, |y| > |x|
    angle = 270 - angle;
    break;
  }

  if (angle >= 360)
  {
    angle -= 360;
  }

  return angle;
}

// ---- Utility Functions ----

// Return absolute value of a fixed-point number.
fixed_t fixed_abs(fixed_t x)
{
  if (x < 0)
  {
    return -x;
  }
  return x;
}

// Return sign of a fixed-point number (-1, 0, or 1).
int fixed_sign(fixed_t x)
{
  if (x > 0)
  {
    return 1;
  }
  if (x < 0)
  {
    return -1;
  }
  return 0;
}

// Return the smaller of two fixed-point values.
fixed_t fixed_min(fixed_t a, fixed_t b)
{
  if (a < b)
  {
    return a;
  }
  return b;
}

// Return the larger of two fixed-point values.
fixed_t fixed_max(fixed_t a, fixed_t b)
{
  if (a > b)
  {
    return a;
  }
  return b;
}

// Clamp a fixed-point value to the inclusive [lo, hi] range.
fixed_t fixed_clamp(fixed_t x, fixed_t lo, fixed_t hi)
{
  if (x < lo)
  {
    return lo;
  }
  if (x > hi)
  {
    return hi;
  }
  return x;
}

// Linearly interpolate from a to b with fixed-point factor t.
fixed_t fixed_lerp(fixed_t a, fixed_t b, fixed_t t)
{
  return a + __multfp(t, b - a);
}

// Approximate 2D distance from dx/dy using a fast metric.
fixed_t fixed_dist_approx(fixed_t dx, fixed_t dy)
{
  fixed_t abs_dx, abs_dy;
  fixed_t max_val, min_val;

  abs_dx = (dx < 0) ? -dx : dx;
  abs_dy = (dy < 0) ? -dy : dy;

  if (abs_dx > abs_dy)
  {
    max_val = abs_dx;
    min_val = abs_dy;
  }
  else
  {
    max_val = abs_dy;
    min_val = abs_dx;
  }

  return max_val + (min_val >> 1);
}

// Compute 2D dot product in fixed-point format.
fixed_t fixed_dot2d(fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2)
{
  return __multfp(x1, x2) + __multfp(y1, y2);
}
