#ifndef FIXEDMATH_H
#define FIXEDMATH_H

// Fixed-Point Math Library
// Provides fixed-point arithmetic for systems without FPU.
// Uses 16.16 fixed-point format (16 bits integer, 16 bits fraction).

// Type definition
typedef int fixed_t;

// Fixed-point format constants
#define FRACBITS 16
#define FRACUNIT 65536 // 1 << 16
#define FRACMASK 65535 // FRACUNIT - 1, i.e. 0xFFFF

// Useful constants in fixed-point
#define FIXED_ZERO 0
#define FIXED_ONE 65536     // FRACUNIT = 1.0
#define FIXED_HALF 32768    // FRACUNIT >> 1 = 0.5
#define FIXED_QUARTER 16384 // FRACUNIT >> 2 = 0.25
#define FIXED_PI 205887     // PI (~3.14159)
#define FIXED_2PI 411775    // 2*PI
#define FIXED_PI_2 102944   // PI/2
#define FIXED_E 178145      // e (~2.71828)

// Conversion functions

// Convert integer to fixed-point.
fixed_t int2fixed(int x);

// Convert fixed-point to integer (truncates toward zero).
int fixed2int(fixed_t x);

// Get fractional part of fixed-point number.
fixed_t fixed_frac(fixed_t x);

// Basic arithmetic functions

// Calculate square root of fixed-point number.
// Uses Newton-Raphson iteration.
fixed_t fixed_sqrt(fixed_t x);

// Trigonometric functions

// Calculate sine of angle.
// Uses lookup table for speed.
fixed_t fixed_sin(int angle);

// Calculate cosine of angle.
// Uses lookup table for speed.
fixed_t fixed_cos(int angle);

// Calculate tangent of angle.
// Computed from sin/cos.
fixed_t fixed_tan(int angle);

// Calculate arc tangent (angle from x,y coordinates).
// Uses lookup table approximation.
int fixed_atan2(fixed_t y, fixed_t x);

// Utility functions

// Absolute value of fixed-point number.
fixed_t fixed_abs(fixed_t x);

// Sign of fixed-point number (-1, 0, or 1).
int fixed_sign(fixed_t x);

// Minimum of two fixed-point numbers.
fixed_t fixed_min(fixed_t a, fixed_t b);

// Maximum of two fixed-point numbers.
fixed_t fixed_max(fixed_t a, fixed_t b);

// Clamp fixed-point value between min and max.
fixed_t fixed_clamp(fixed_t x, fixed_t lo, fixed_t hi);

// Linear interpolation between two fixed-point values.
fixed_t fixed_lerp(fixed_t a, fixed_t b, fixed_t t);

// Calculate distance between two 2D points (approximation).
// Uses fast approximation: max(dx, dy) + min(dx, dy)/2
fixed_t fixed_dist_approx(fixed_t dx, fixed_t dy);

// Calculate dot product of two 2D vectors.
fixed_t fixed_dot2d(fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2);

#endif // FIXEDMATH_H
