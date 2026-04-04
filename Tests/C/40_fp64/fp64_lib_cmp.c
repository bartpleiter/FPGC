// Test: fp64_cmp C library function
// extra_sources=Software/C/userlib/src/fixed64_asm.asm,Software/C/userlib/src/fixed64.c
// compile_flags=-I Software/C/userlib/include

#include <fixed64.h>

int main(void)
{
    struct fp64 a, b, c;

    fp64_make(&a, 5, 0);
    fp64_make(&b, 3, 0);
    fp64_make(&c, 5, 0);

    int r1 = fp64_cmp(&a, &b);  // 5 > 3: positive
    int r2 = fp64_cmp(&b, &a);  // 3 < 5: negative
    int r3 = fp64_cmp(&a, &c);  // 5 == 5: zero

    int result = 0;
    if (r1 > 0) result += 1;
    if (r2 < 0) result += 2;
    if (r3 == 0) result += 4;

    return result; // expected=0x07
}

void interrupt(void) {}
