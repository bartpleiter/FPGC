/*
 * Test: stdlib conversion and utility functions
 * Tests atoi, abs, rand
 */

#define COMMON_STDLIB
#include "libs/common/common.h"

int main() {
    /* Test atoi */
    if (atoi("123") != 123) return 1;
    if (atoi("-45") != -45) return 2;
    if (atoi("  42") != 42) return 3;
    if (atoi("+7") != 7) return 4;
    if (atoi("0") != 0) return 5;
    
    /* Test abs */
    if (abs(-10) != 10) return 6;
    if (abs(10) != 10) return 77;
    if (abs(0) != 0) return 8;
    
    /* Test rand/srand - basic functionality */
    srand(12345);
    int r1 = rand();
    int r2 = rand();
    
    /* Different calls should produce different values */
    if (r1 == r2) return 9;
    
    /* Same seed should produce same sequence */
    srand(12345);
    int r3 = rand();
    if (r1 != r3) return 10;
    
    /* Test int_min/int_max functions */
    if (int_min(5, 10) != 5) return 11;
    if (int_max(5, 10) != 10) return 12;
    
    /* All tests passed */
    return 7; // expected=0x07
}

void interrupt()
{
}
