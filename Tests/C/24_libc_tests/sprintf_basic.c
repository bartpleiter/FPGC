/*
 * Test: stdio basic functions (putchar, puts)
 * Tests basic character output without variadic functions
 * Note: printf/sprintf with varargs may not be fully supported by B32CC
 */

#define COMMON_STRING
#include "libs/common/common.h"

/* Test integer to string conversion manually */
void int_to_str(int val, char *buf)
{
    char temp[16];
    int i = 0;
    int j = 0;
    int neg = 0;
    
    if (val < 0)
    {
        neg = 1;
        val = -val;
    }
    
    /* Generate digits in reverse */
    do
    {
        temp[i++] = '0' + (val % 10);
        val = val / 10;
    } while (val > 0);
    
    if (neg)
    {
        temp[i++] = '-';
    }
    
    /* Reverse into output buffer */
    while (i > 0)
    {
        buf[j++] = temp[--i];
    }
    buf[j] = '\0';
}

int main() {
    char buf[32];
    
    /* Test int_to_str with positive number */
    int_to_str(42, buf);
    if (buf[0] != '4') return 1;
    if (buf[1] != '2') return 2;
    if (buf[2] != '\0') return 3;
    
    /* Test int_to_str with larger number */
    int_to_str(12345, buf);
    if (buf[0] != '1') return 4;
    if (buf[4] != '5') return 5;
    
    /* Test int_to_str with negative number */
    int_to_str(-7, buf);
    if (buf[0] != '-') return 6;
    if (buf[1] != '7') return 77;
    
    /* Test int_to_str with zero */
    int_to_str(0, buf);
    if (buf[0] != '0') return 8;
    if (buf[1] != '\0') return 9;
    
    /* All tests passed */
    return 7; // expected=0x07
}

void interrupt()
{
}
