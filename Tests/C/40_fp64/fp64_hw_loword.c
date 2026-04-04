// Test: FP64 hardware lo word readback
// Verifies that fractional parts survive load/store roundtrip
// extra_sources=Software/C/userlib/src/fixed64_asm.asm

extern void fp64_hw_load6(int hi, int lo);
extern int  fp64_hw_store_hi6(void);
extern int  fp64_hw_store_lo6(void);

int main(void)
{
    // Load 0.5 in Q32.32 = {0, 0x80000000}
    fp64_hw_load6(0, 0x80000000);
    int hi = fp64_hw_store_hi6();
    int lo = fp64_hw_store_lo6();

    // hi should be 0, lo should be 0x80000000
    // Return: hi should be 0, check lo top byte
    if (hi != 0) return 1;
    // lo >> 24 should be 0x80
    int result = (unsigned int)lo >> 24;
    return result; // expected=0x80
}

void interrupt(void) {}
