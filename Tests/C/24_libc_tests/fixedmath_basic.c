/*
 * Test: Fixed-point math library
 * Tests basic conversion and arithmetic
 */

#define COMMON_FIXEDMATH
#include "libs/common/common.h"

int main() {
    fixed_t a, b, c;
    
    /* Test conversion macros */
    a = int2fixed(5);  /* 5.0 */
    if (fixed2int(a) != 5) return 1;
    
    b = int2fixed(3);  /* 3.0 */
    if (fixed2int(b) != 3) return 2;
    
    /* Test addition (uses regular + since same scale) */
    c = a + b;  /* 8.0 */
    if (fixed2int(c) != 8) return 3;
    
    /* Test subtraction */
    c = a - b;  /* 2.0 */
    if (fixed2int(c) != 2) return 4;
    
    /* Test multiplication */
    c = __multfp(a, b);  /* 15.0 */
    if (fixed2int(c) != 15) return 5;
    
    /* Test division */
    c = __divfp(a, b);  /* ~1.666... */
    if (fixed2int(c) != 1) return 6;
    
    /* Test multiplication with fraction: 2.5 * 2 = 5 */
    a = int2fixed(2) + FIXED_HALF;  /* 2.5 */
    b = int2fixed(2);
    c = __multfp(a, b);  /* 5.0 */
    if (fixed2int(c) != 5) return 77;
    
    /* Test fixed_abs */
    a = int2fixed(-10);
    if (fixed2int(fixed_abs(a)) != 10) return 8;
    
    /* Test sin/cos at known angles */
    /* sin(0) = 0 */
    if (fixed_sin(0) != 0) return 9;
    
    /* sin(90) = 1.0 = FRACUNIT */
    if (fixed_sin(90) != FRACUNIT) return 10;
    
    /* cos(0) = 1.0 = FRACUNIT */
    if (fixed_cos(0) != FRACUNIT) return 11;
    
    /* cos(90) = 0 */
    if (fixed_cos(90) != 0) return 12;
    
    /* sin(180) = 0 */
    if (fixed_sin(180) != 0) return 13;
    
    /* sin(270) = -1.0 = -FRACUNIT */
    if (fixed_sin(270) != -FRACUNIT) return 14;
    
    /* All tests passed */
    return 7; // expected=0x07
}

void interrupt()
{
}
