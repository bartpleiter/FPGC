#include "libs/common/fixedmath.h"

/*
 * Fixed-Point Math Library Implementation
 * 
 * Uses 16.16 fixed-point format.
 * Optimized for systems without FPU.
 */

/*
 * Sine lookup table (0-90 degrees, in fixed-point).
 * Values are sin(angle) * FRACUNIT.
 * The table covers 0-90 degrees in 1-degree increments.
 * Other quadrants are computed by symmetry.
 */
static const fixed_t sin_table[91] = {
    0,      /* 0 */
    1143,   /* 1 */
    2287,   /* 2 */
    3429,   /* 3 */
    4571,   /* 4 */
    5711,   /* 5 */
    6850,   /* 6 */
    7986,   /* 7 */
    9120,   /* 8 */
    10252,  /* 9 */
    11380,  /* 10 */
    12504,  /* 11 */
    13625,  /* 12 */
    14742,  /* 13 */
    15854,  /* 14 */
    16961,  /* 15 */
    18064,  /* 16 */
    19160,  /* 17 */
    20251,  /* 18 */
    21336,  /* 19 */
    22414,  /* 20 */
    23486,  /* 21 */
    24550,  /* 22 */
    25606,  /* 23 */
    26655,  /* 24 */
    27696,  /* 25 */
    28729,  /* 26 */
    29752,  /* 27 */
    30767,  /* 28 */
    31772,  /* 29 */
    32768,  /* 30 */
    33753,  /* 31 */
    34728,  /* 32 */
    35693,  /* 33 */
    36647,  /* 34 */
    37589,  /* 35 */
    38521,  /* 36 */
    39440,  /* 37 */
    40347,  /* 38 */
    41243,  /* 39 */
    42125,  /* 40 */
    42995,  /* 41 */
    43852,  /* 42 */
    44695,  /* 43 */
    45525,  /* 44 */
    46340,  /* 45 */
    47142,  /* 46 */
    47929,  /* 47 */
    48702,  /* 48 */
    49460,  /* 49 */
    50203,  /* 50 */
    50931,  /* 51 */
    51643,  /* 52 */
    52339,  /* 53 */
    53019,  /* 54 */
    53683,  /* 55 */
    54331,  /* 56 */
    54963,  /* 57 */
    55577,  /* 58 */
    56175,  /* 59 */
    56755,  /* 60 */
    57319,  /* 61 */
    57864,  /* 62 */
    58393,  /* 63 */
    58903,  /* 64 */
    59395,  /* 65 */
    59870,  /* 66 */
    60326,  /* 67 */
    60763,  /* 68 */
    61183,  /* 69 */
    61583,  /* 70 */
    61965,  /* 71 */
    62328,  /* 72 */
    62672,  /* 73 */
    62997,  /* 74 */
    63302,  /* 75 */
    63589,  /* 76 */
    63856,  /* 77 */
    64103,  /* 78 */
    64331,  /* 79 */
    64540,  /* 80 */
    64729,  /* 81 */
    64898,  /* 82 */
    65047,  /* 83 */
    65176,  /* 84 */
    65286,  /* 85 */
    65376,  /* 86 */
    65446,  /* 87 */
    65496,  /* 88 */
    65526,  /* 89 */
    65536   /* 90 = FRACUNIT */
};

/*
 * Arctangent lookup table for atan2.
 * Maps atan(y/x) * 256 / 45 for y/x from 0 to 1.
 * Index is (y/x) * 256, value is angle * 256 / 45.
 */
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
    64  /* 45 degrees at index 256 */
};

/* Conversion functions */

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

/* Basic arithmetic functions */

fixed_t fixed_mul(fixed_t a, fixed_t b)
{
    /*
     * TODO: use multi-cycle ALU for this instead!
     *
     * Multiply two 16.16 fixed-point numbers.
     * Result = (a * b) >> FRACBITS
     * 
     * We need 64-bit intermediate to avoid overflow.
     * Since we don't have 64-bit types, we split the multiplication.
     */
    int sign = 1;
    unsigned int ua, ub;
    unsigned int a_hi, a_lo, b_hi, b_lo;
    unsigned int result;

    /* Handle signs */
    if (a < 0)
    {
        a = -a;
        sign = -sign;
    }
    if (b < 0)
    {
        b = -b;
        sign = -sign;
    }

    ua = (unsigned int)a;
    ub = (unsigned int)b;

    /* Split into high and low 16-bit parts */
    a_hi = ua >> 16;
    a_lo = ua & 0xFFFF;
    b_hi = ub >> 16;
    b_lo = ub & 0xFFFF;

    /*
     * Multiply parts:
     * ua * ub = (a_hi * 2^16 + a_lo) * (b_hi * 2^16 + b_lo)
     *         = a_hi * b_hi * 2^32 + (a_hi * b_lo + a_lo * b_hi) * 2^16 + a_lo * b_lo
     * 
     * After >> 16:
     * = a_hi * b_hi * 2^16 + (a_hi * b_lo + a_lo * b_hi) + (a_lo * b_lo >> 16)
     */
    result = (a_hi * b_hi) << 16;
    result += a_hi * b_lo;
    result += a_lo * b_hi;
    result += (a_lo * b_lo) >> 16;

    return (sign < 0) ? -(fixed_t)result : (fixed_t)result;
}

fixed_t fixed_div(fixed_t a, fixed_t b)
{
    /*
     * TODO: use multi-cycle ALU for this instead!
     *
     * Divide two 16.16 fixed-point numbers.
     * Result = (a << FRACBITS) / b
     * 
     * We need to be careful about overflow.
     */
    int sign = 1;
    unsigned int ua, ub;
    unsigned int result;
    int shift;

    if (b == 0)
    {
        /* Return max/min value on divide by zero */
        return (a >= 0) ? 0x7FFFFFFF : -0x7FFFFFFF;
    }

    /* Handle signs */
    if (a < 0)
    {
        a = -a;
        sign = -sign;
    }
    if (b < 0)
    {
        b = -b;
        sign = -sign;
    }

    ua = (unsigned int)a;
    ub = (unsigned int)b;

    /*
     * Simple approach: shift a left as much as possible without overflow,
     * then divide, then adjust.
     */
    result = 0;
    shift = FRACBITS;

    /* Shift a left as much as possible */
    while ((ua & 0x80000000) == 0 && shift > 0)
    {
        ua <<= 1;
        shift--;
    }

    result = ua / ub;
    result <<= shift;

    return (sign < 0) ? -(fixed_t)result : (fixed_t)result;
}

fixed_t fixed_sqrt(fixed_t x)
{
    /*
     * Newton-Raphson square root.
     * For fixed-point, we compute sqrt(x * FRACUNIT) to maintain precision.
     */
    fixed_t guess, prev_guess;
    int i;

    if (x <= 0)
    {
        return 0;
    }

    /* Initial guess: x/2 or sqrt(x) approximation */
    if (x > FRACUNIT)
    {
        guess = x >> 1;
    }
    else
    {
        guess = FRACUNIT;
    }

    /* Newton-Raphson iterations: guess = (guess + x/guess) / 2 */
    for (i = 0; i < 16; i++)
    {
        prev_guess = guess;
        guess = (guess + fixed_div(x, guess)) >> 1;

        /* Check for convergence */
        if (guess == prev_guess)
        {
            break;
        }
    }

    return guess;
}

/* Trigonometric functions */

fixed_t fixed_sin(int angle)
{
    int quadrant;
    int idx;
    fixed_t result;

    /* Normalize angle to 0-359 */
    angle = angle % 360;
    if (angle < 0)
    {
        angle += 360;
    }

    /* Determine quadrant and index */
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

fixed_t fixed_cos(int angle)
{
    /* cos(x) = sin(x + 90) */
    return fixed_sin(angle + 90);
}

fixed_t fixed_tan(int angle)
{
    fixed_t s, c;

    s = fixed_sin(angle);
    c = fixed_cos(angle);

    if (c == 0)
    {
        /* Return max/min value for tan(90), tan(270) */
        return (s >= 0) ? 0x7FFFFFFF : -0x7FFFFFFF;
    }

    return fixed_div(s, c);
}

int fixed_atan2(fixed_t y, fixed_t x)
{
    /*
     * Calculate angle from x,y coordinates using lookup table.
     * Returns angle in degrees (0-359).
     */
    int angle;
    fixed_t abs_x, abs_y;
    int octant;
    unsigned int idx;
    fixed_t ratio;

    /* Handle special cases */
    if (x == 0)
    {
        if (y > 0) return 90;
        if (y < 0) return 270;
        return 0;
    }

    if (y == 0)
    {
        return (x > 0) ? 0 : 180;
    }

    /* Get absolute values */
    abs_x = (x < 0) ? -x : x;
    abs_y = (y < 0) ? -y : y;

    /* Determine octant */
    octant = 0;
    if (y < 0) octant |= 4;
    if (x < 0) octant |= 2;
    if (abs_y > abs_x) octant |= 1;

    /* Calculate ratio for table lookup */
    if (abs_y > abs_x)
    {
        ratio = fixed_div(abs_x << 8, abs_y);  /* Scale to 0-256 range */
    }
    else
    {
        ratio = fixed_div(abs_y << 8, abs_x);
    }

    /* Clamp index */
    idx = (unsigned int)fixed2int(ratio);
    if (idx > 256)
    {
        idx = 256;
    }

    /* Get base angle from table (scaled by 256/45) */
    angle = (atan_table[idx] * 45) >> 6;

    /* Adjust for octant */
    switch (octant)
    {
        case 0: /* +x, +y, |x| >= |y| */
            break;
        case 1: /* +x, +y, |y| > |x| */
            angle = 90 - angle;
            break;
        case 2: /* -x, +y, |x| >= |y| */
            angle = 180 - angle;
            break;
        case 3: /* -x, +y, |y| > |x| */
            angle = 90 + angle;
            break;
        case 4: /* +x, -y, |x| >= |y| */
            angle = 360 - angle;
            break;
        case 5: /* +x, -y, |y| > |x| */
            angle = 270 + angle;
            break;
        case 6: /* -x, -y, |x| >= |y| */
            angle = 180 + angle;
            break;
        case 7: /* -x, -y, |y| > |x| */
            angle = 270 - angle;
            break;
    }

    /* Normalize to 0-359 */
    if (angle >= 360)
    {
        angle -= 360;
    }

    return angle;
}

/* Utility functions */

fixed_t fixed_abs(fixed_t x)
{
    if (x < 0)
    {
        return -x;
    }
    return x;
}

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

fixed_t fixed_min(fixed_t a, fixed_t b)
{
    if (a < b)
    {
        return a;
    }
    return b;
}

fixed_t fixed_max(fixed_t a, fixed_t b)
{
    if (a > b)
    {
        return a;
    }
    return b;
}

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

fixed_t fixed_lerp(fixed_t a, fixed_t b, fixed_t t)
{
    /* Linear interpolation: a + t * (b - a) */
    return a + fixed_mul(t, b - a);
}

fixed_t fixed_dist_approx(fixed_t dx, fixed_t dy)
{
    /*
     * Fast distance approximation.
     * max(|dx|, |dy|) + min(|dx|, |dy|) / 2
     * This is accurate to within about 8%.
     */
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

fixed_t fixed_dot2d(fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2)
{
    /* Dot product: x1*x2 + y1*y2 */
    return fixed_mul(x1, x2) + fixed_mul(y1, y2);
}
