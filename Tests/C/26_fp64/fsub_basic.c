// Test __fsub intrinsic
// {0,10} - {0,3} = {0,7}

int main() {
    __fld(0, 0, 10);    // f0 = {0, 10}
    __fld(1, 0, 3);     // f1 = {0, 3}
    __fsub(2, 0, 1);    // f2 = f0 - f1 = {0, 7}
    int hi = __fsthi(2);
    int lo = __fstlo(2);
    return hi * 100 + lo; // expected=0x07
}

void interrupt() {}
