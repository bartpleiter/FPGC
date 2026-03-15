// Test __fld, __fsthi, __fstlo intrinsics
// Load {5, 3} into f0, read back hi and lo

int main() {
    __fld(0, 5, 3);
    int hi = __fsthi(0);
    int lo = __fstlo(0);
    // hi should be 5, lo should be 3
    return hi * 10 + lo; // expected=0x35
}

void interrupt() {}
