#ifndef FIXEDMATH_H
#define FIXEDMATH_H

/*
 * Fixed-Point Math Library
 * Q16.16 format (16 bits integer, 16 bits fraction).
 * Uses hardware multfp/divfp instructions via __builtin_multfp/__builtin_divfp.
 */

typedef int fixed_t;

#define FRACBITS 16
#define FRACUNIT 65536
#define FRACMASK 65535

#define FIXED_ZERO    0
#define FIXED_ONE     65536
#define FIXED_HALF    32768
#define FIXED_QUARTER 16384
#define FIXED_PI      205887
#define FIXED_2PI     411775
#define FIXED_PI_2    102944
#define FIXED_E       178145

/* Conversion */
fixed_t int2fixed(int x);
int     fixed2int(fixed_t x);
fixed_t fixed_frac(fixed_t x);

/* Arithmetic */
fixed_t fixed_sqrt(fixed_t x);

/* Trigonometry (angles in degrees) */
fixed_t fixed_sin(int angle);
fixed_t fixed_cos(int angle);
fixed_t fixed_tan(int angle);
int     fixed_atan2(fixed_t y, fixed_t x);

/* Utility */
fixed_t fixed_abs(fixed_t x);
int     fixed_sign(fixed_t x);
fixed_t fixed_min(fixed_t a, fixed_t b);
fixed_t fixed_max(fixed_t a, fixed_t b);
fixed_t fixed_clamp(fixed_t x, fixed_t lo, fixed_t hi);
fixed_t fixed_lerp(fixed_t a, fixed_t b, fixed_t t);
fixed_t fixed_dist_approx(fixed_t dx, fixed_t dy);
fixed_t fixed_dot2d(fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2);

#endif /* FIXEDMATH_H */
