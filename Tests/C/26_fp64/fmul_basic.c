// Test __fmul intrinsic
// {2, 0} * {3, 0} = {6, 0} in 32.32 fixed-point

int main() {
    __fld(0, 2, 0);     // f0 = 2.0
    __fld(1, 3, 0);     // f1 = 3.0
    __fmul(2, 0, 1);    // f2 = f0 * f1 = 6.0 = {6, 0}
    int hi = __fsthi(2);
    int lo = __fstlo(2);
    return hi * 16 + lo; // expected=0x60
}

void interrupt() {}
