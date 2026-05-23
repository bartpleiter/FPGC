/*
 * fixedmath.c — Q16.16 fixed-point math library.
 * Uses hardware multfp/divfp via __builtin_multfp/__builtin_divfp.
 */

#include <fixedmath.h>

/* Sine lookup table: sin(0..90) * FRACUNIT, 1-degree increments */
static const fixed_t sin_table[91] = {
    0, 1143, 2287, 3429, 4571, 5711, 6850, 7986, 9120, 10252,
    11380, 12504, 13625, 14742, 15854, 16961, 18064, 19160, 20251, 21336,
    22414, 23486, 24550, 25606, 26655, 27696, 28729, 29752, 30767, 31772,
    32768, 33753, 34728, 35693, 36647, 37589, 38521, 39440, 40347, 41243,
    42125, 42995, 43852, 44695, 45525, 46340, 47142, 47929, 48702, 49460,
    50203, 50931, 51643, 52339, 53019, 53683, 54331, 54963, 55577, 56175,
    56755, 57319, 57864, 58393, 58903, 59395, 59870, 60326, 60763, 61183,
    61583, 61965, 62328, 62672, 62997, 63302, 63589, 63856, 64103, 64331,
    64540, 64729, 64898, 65047, 65176, 65286, 65376, 65446, 65496, 65526,
    65536
};

/* Arctangent lookup table for atan2 */
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
    64
};

/* ---- Conversion ---- */

fixed_t int2fixed(int x)
{
    return (fixed_t)(x << FRACBITS);
}

int fixed2int(fixed_t x)
{
    return x >> FRACBITS;
}

fixed_t fixed_frac(fixed_t x)
{
    return x & FRACMASK;
}

/* ---- Arithmetic ---- */

fixed_t fixed_sqrt(fixed_t x)
{
    fixed_t guess, prev_guess;
    int i;

    if (x <= 0)
        return 0;

    guess = (x > FRACUNIT) ? x >> 1 : FRACUNIT;

    for (i = 0; i < 16; i++)
    {
        prev_guess = guess;
        guess = (guess + __builtin_divfp(x, guess)) >> 1;
        if (guess == prev_guess)
            break;
    }

    return guess;
}

/* ---- Trigonometry ---- */

fixed_t fixed_sin(int angle)
{
    int quadrant, idx;

    angle = angle % 360;
    if (angle < 0)
        angle += 360;

    quadrant = angle / 90;
    idx = angle % 90;

    switch (quadrant)
    {
    case 0: return sin_table[idx];
    case 1: return sin_table[90 - idx];
    case 2: return -sin_table[idx];
    case 3: return -sin_table[90 - idx];
    default: return 0;
    }
}

fixed_t fixed_cos(int angle)
{
    return fixed_sin(angle + 90);
}

fixed_t fixed_tan(int angle)
{
    fixed_t s = fixed_sin(angle);
    fixed_t c = fixed_cos(angle);

    if (c == 0)
        return (s >= 0) ? 0x7FFFFFFF : -0x7FFFFFFF;

    return __builtin_divfp(s, c);
}

int fixed_atan2(fixed_t y, fixed_t x)
{
    int angle;
    fixed_t abs_x, abs_y;
    int octant;
    unsigned int idx;
    fixed_t ratio;

    if (x == 0)
    {
        if (y > 0) return 90;
        if (y < 0) return 270;
        return 0;
    }
    if (y == 0)
        return (x > 0) ? 0 : 180;

    abs_x = (x < 0) ? -x : x;
    abs_y = (y < 0) ? -y : y;

    octant = 0;
    if (y < 0) octant |= 4;
    if (x < 0) octant |= 2;
    if (abs_y > abs_x) octant |= 1;

    if (abs_y > abs_x)
        ratio = __builtin_divfp(abs_x << 8, abs_y);
    else
        ratio = __builtin_divfp(abs_y << 8, abs_x);

    idx = (unsigned int)fixed2int(ratio);
    if (idx > 256)
        idx = 256;

    angle = (atan_table[idx] * 45) >> 6;

    switch (octant)
    {
    case 0: break;
    case 1: angle = 90 - angle; break;
    case 2: angle = 180 - angle; break;
    case 3: angle = 90 + angle; break;
    case 4: angle = 360 - angle; break;
    case 5: angle = 270 + angle; break;
    case 6: angle = 180 + angle; break;
    case 7: angle = 270 - angle; break;
    }

    if (angle >= 360)
        angle -= 360;

    return angle;
}

/* ---- Utility ---- */

fixed_t fixed_abs(fixed_t x)
{
    return (x < 0) ? -x : x;
}

int fixed_sign(fixed_t x)
{
    if (x > 0) return 1;
    if (x < 0) return -1;
    return 0;
}

fixed_t fixed_min(fixed_t a, fixed_t b)
{
    return (a < b) ? a : b;
}

fixed_t fixed_max(fixed_t a, fixed_t b)
{
    return (a > b) ? a : b;
}

fixed_t fixed_clamp(fixed_t x, fixed_t lo, fixed_t hi)
{
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

fixed_t fixed_lerp(fixed_t a, fixed_t b, fixed_t t)
{
    return a + __builtin_multfp(t, b - a);
}

fixed_t fixed_dist_approx(fixed_t dx, fixed_t dy)
{
    fixed_t abs_dx = (dx < 0) ? -dx : dx;
    fixed_t abs_dy = (dy < 0) ? -dy : dy;
    fixed_t max_val = (abs_dx > abs_dy) ? abs_dx : abs_dy;
    fixed_t min_val = (abs_dx > abs_dy) ? abs_dy : abs_dx;
    return max_val + (min_val >> 1);
}

fixed_t fixed_dot2d(fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2)
{
    return __builtin_multfp(x1, x2) + __builtin_multfp(y1, y2);
}
