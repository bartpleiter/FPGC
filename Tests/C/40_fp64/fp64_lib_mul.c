// Test: fp64_mul C library function
// Tests struct-based FP64 multiplication through the C library
// extra_sources=Software/C/userlib/src/fixed64_asm.asm,Software/C/userlib/src/fixed64.c
// compile_flags=-I Software/C/userlib/include

#include <fixed64.h>

int main(void)
{
    struct fp64 a, b, result;

    // 3.0 * 4.0 = 12.0
    fp64_make(&a, 3, 0);
    fp64_make(&b, 4, 0);
    fp64_mul(&result, &a, &b);

    return result.hi; // expected=0x0C
}

void interrupt(void) {}
