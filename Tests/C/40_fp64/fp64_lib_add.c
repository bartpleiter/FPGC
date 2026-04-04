// Test: fp64_add C library function
// Tests struct-based FP64 addition through the C library
// extra_sources=Software/C/userlib/src/fixed64_asm.asm,Software/C/userlib/src/fixed64.c
// compile_flags=-I Software/C/userlib/include

#include <fixed64.h>

int main(void)
{
    struct fp64 a, b, result;

    // 2.0 + 3.0 = 5.0
    fp64_make(&a, 2, 0);
    fp64_make(&b, 3, 0);
    fp64_add(&result, &a, &b);

    return result.hi; // expected=0x05
}

void interrupt(void) {}
