// Test: FP64 hardware multiply via assembly wrappers
// Tests that fp64_hw_mul primitive works correctly
// extra_sources=Software/C/userlib/src/fixed64_asm.asm

extern void fp64_hw_load6(int hi, int lo);
extern void fp64_hw_load7(int hi, int lo);
extern int  fp64_hw_store_hi6(void);
extern void fp64_hw_mul66_7(void);

int main(void)
{
    // 3.0 * 4.0 = 12.0 in Q32.32
    fp64_hw_load6(3, 0);
    fp64_hw_load7(4, 0);
    fp64_hw_mul66_7();
    int hi = fp64_hw_store_hi6();
    // hi should be 12
    return hi; // expected=0x0C
}

void interrupt(void) {}
