// Test: FP64 hardware add via assembly wrappers
// Tests that fp64_hw_load/store/add primitives work correctly
// extra_sources=Software/C/userlib/src/fixed64_asm.asm

extern void fp64_hw_load6(int hi, int lo);
extern void fp64_hw_load7(int hi, int lo);
extern int  fp64_hw_store_hi6(void);
extern int  fp64_hw_store_lo6(void);
extern void fp64_hw_add66_7(void);

int main(void)
{
    // 2.0 + 3.0 = 5.0 in Q32.32
    // 2.0 = {2, 0}, 3.0 = {3, 0}
    fp64_hw_load6(2, 0);
    fp64_hw_load7(3, 0);
    fp64_hw_add66_7();
    int hi = fp64_hw_store_hi6();
    // hi should be 5
    return hi; // expected=0x05
}

void interrupt(void) {}
