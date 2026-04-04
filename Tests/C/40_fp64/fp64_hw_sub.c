// Test: FP64 hardware subtract via assembly wrappers
// extra_sources=Software/C/userlib/src/fixed64_asm.asm

extern void fp64_hw_load6(int hi, int lo);
extern void fp64_hw_load7(int hi, int lo);
extern int  fp64_hw_store_hi6(void);
extern void fp64_hw_sub66_7(void);

int main(void)
{
    // 10.0 - 3.0 = 7.0 in Q32.32
    fp64_hw_load6(10, 0);
    fp64_hw_load7(3, 0);
    fp64_hw_sub66_7();
    int hi = fp64_hw_store_hi6();
    return hi; // expected=0x07
}

void interrupt(void) {}
