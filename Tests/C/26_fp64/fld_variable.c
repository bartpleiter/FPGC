// Test __fld with variable arguments
// Load hi/lo from variables, add two FP values, read back

int main() {
    int a_hi = 1;
    int a_lo = 500;
    int b_hi = 2;
    int b_lo = 300;

    __fld(0, a_hi, a_lo);   // f0 = {1, 500}
    __fld(1, b_hi, b_lo);   // f1 = {2, 300}
    __fadd(2, 0, 1);        // f2 = f0 + f1 = {3, 800}
    int hi = __fsthi(2);
    int lo = __fstlo(2);
    // hi=3, lo=800 (0x320)
    return hi; // expected=0x03
}

void interrupt() {}
