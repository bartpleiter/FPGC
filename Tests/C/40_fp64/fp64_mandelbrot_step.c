// Test: FP64 chained operations (like Mandelbrot z^2 + c)
// Tests z_re^2 - z_im^2 + c_re pattern
// extra_sources=Software/C/userlib/src/fixed64_asm.asm,Software/C/userlib/src/fixed64.c
// compile_flags=-I Software/C/userlib/include

#include <fixed64.h>

int main(void)
{
    struct fp64 z_re, z_im, c_re;
    struct fp64 zr2, zi2, tmp, new_re;

    // z = (1.0, 0.5), c = (0.25, 0)
    fp64_make(&z_re, 1, 0);
    fp64_make(&z_im, 0, 0x80000000);  // 0.5
    fp64_make(&c_re, 0, 0x40000000);  // 0.25

    // z_re^2 = 1.0
    fp64_mul(&zr2, &z_re, &z_re);
    // z_im^2 = 0.25
    fp64_mul(&zi2, &z_im, &z_im);
    // z_re^2 - z_im^2 = 0.75
    fp64_sub(&tmp, &zr2, &zi2);
    // + c_re = 1.0
    fp64_add(&new_re, &tmp, &c_re);

    // new_re should be 1.0 = {1, 0}
    if (new_re.hi != 1) return new_re.hi + 0x80;
    if (new_re.lo != 0) return 0xEE;
    return 0x01; // expected=0x01
}

void interrupt(void) {}
