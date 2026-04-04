// Test: fp64_from_int and fp64_to_int roundtrip
// extra_sources=Software/C/userlib/src/fixed64_asm.asm,Software/C/userlib/src/fixed64.c
// compile_flags=-I Software/C/userlib/include

#include <fixed64.h>

int main(void)
{
    struct fp64 a;

    fp64_from_int(&a, 42);
    int back = fp64_to_int(&a);

    if (back != 42) return 0xEE;

    // Also test negative
    fp64_from_int(&a, -5);
    back = fp64_to_int(&a);
    if (back != -5) return 0xDD;

    return 0x2A; // expected=0x2A
}

void interrupt(void) {}
