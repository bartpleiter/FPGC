// Test __fadd intrinsic
// {0,5} + {0,3} = {0,8}

int main() {
    __fld(0, 0, 5);     // f0 = {0, 5}
    __fld(1, 0, 3);     // f1 = {0, 3}
    __fadd(2, 0, 1);    // f2 = f0 + f1 = {0, 8}
    int hi = __fsthi(2);
    int lo = __fstlo(2);
    return hi * 100 + lo; // expected=0x08
}

void interrupt() {}
