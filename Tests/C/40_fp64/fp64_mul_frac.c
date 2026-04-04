// Test: FP64 multiply with fractional values
// Tests 0.5 * 0.5 = 0.25 to verify fractional precision
// extra_sources=Software/C/userlib/src/fixed64_asm.asm,Software/C/userlib/src/fixed64.c
// compile_flags=-I Software/C/userlib/include

#include <fixed64.h>

int main(void)
{
    struct fp64 a, result;

    // 0.5 in Q32.32 = {0, 0x80000000}
    fp64_make(&a, 0, 0x80000000);

    // 0.5 * 0.5 = 0.25
    // 0.25 in Q32.32 = {0, 0x40000000}
    fp64_mul(&result, &a, &a);

    if (result.hi != 0) return 1;
    // Check lo: should be 0x40000000; extract bits 31..24
    int check = (unsigned int)result.lo >> 24;
    return check; // expected=0x40
}

void interrupt(void) {}
