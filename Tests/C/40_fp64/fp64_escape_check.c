// Test: FP64 magnitude check (|z|^2 >= 4 escape condition)
// This is the critical Mandelbrot escape test
// extra_sources=Software/C/userlib/src/fixed64_asm.asm,Software/C/userlib/src/fixed64.c
// compile_flags=-I Software/C/userlib/include

#include <fixed64.h>

int main(void)
{
    struct fp64 zr2, zi2, mag, four;

    // z = (1.5, 1.5): |z|^2 = 2.25 + 2.25 = 4.5 (should escape)
    // zr2 = 1.5^2 = 2.25 = {2, 0x40000000}
    fp64_make(&zr2, 2, 0x40000000);
    // zi2 = 1.5^2 = 2.25
    fp64_make(&zi2, 2, 0x40000000);
    fp64_make(&four, 4, 0);

    // mag = zr2 + zi2 = 4.5
    fp64_add(&mag, &zr2, &zi2);

    int escaped = (fp64_cmp(&mag, &four) >= 0) ? 1 : 0;
    if (!escaped) return 0xEE;

    // Now test: z = (1.0, 1.0): |z|^2 = 2.0 (should NOT escape)
    fp64_make(&zr2, 1, 0);
    fp64_make(&zi2, 1, 0);
    fp64_add(&mag, &zr2, &zi2);

    int not_escaped = (fp64_cmp(&mag, &four) < 0) ? 1 : 0;
    if (!not_escaped) return 0xDD;

    return 0x42; // expected=0x42
}

void interrupt(void) {}
