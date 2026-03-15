/*
 * Test: Fixed-point intrinsics with complex arguments
 * Tests __multfp and __divfp with expressions as arguments
 */

#define COMMON_FIXEDMATH
#include "libs/common/common.h"

int main() {
    int x = 5;
    int y = 3;
    
    /* Test 1: Simple variables (should work) */
    int a = x << 16;
    int b = y << 16;
    int r1 = __multfp(a, b);
    if (fixed2int(r1) != 15) return 1;
    
    /* Test 2: Inline shift expressions as arguments */
    int r2 = __multfp(x << 16, y << 16);
    if (fixed2int(r2) != 15) return 2;
    
    /* Test 3: Division with inline shift */
    int r3 = __divfp(x << 16, y << 16);
    if (fixed2int(r3) != 1) return 3;  /* 5/3 = 1.666, truncates to 1 */
    
    /* Test 4: Arithmetic in arguments */
    int r4 = __multfp((x + 1) << 16, (y - 1) << 16);  /* 6 * 2 = 12 */
    if (fixed2int(r4) != 12) return 4;
    
    /* Test 5: int2fixed macro (simple) */
    int r5 = __divfp(int2fixed(x), int2fixed(y));  /* 5 / 3 = 1.666 */
    if (fixed2int(r5) != 1) return 5;
    
    /* Test 6: Arithmetic expression inside int2fixed */
    int r6 = __divfp(int2fixed(2 * x), int2fixed(y));  /* 10 / 3 = 3.33 */
    if (fixed2int(r6) != 3) return 6;
    
    /* All tests passed */
    return 7; // expected=0x07
}

void interrupt()
{
}
