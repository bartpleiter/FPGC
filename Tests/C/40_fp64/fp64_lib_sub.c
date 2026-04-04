// Test: fp64_sub C library function
// extra_sources=Software/C/userlib/src/fixed64_asm.asm,Software/C/userlib/src/fixed64.c
// compile_flags=-I Software/C/userlib/include

#include <fixed64.h>

int main(void)
{
    struct fp64 a, b, result;

    // 10.0 - 3.0 = 7.0
    fp64_make(&a, 10, 0);
    fp64_make(&b, 3, 0);
    fp64_sub(&result, &a, &b);

    return result.hi; // expected=0x07
}

void interrupt(void) {}
