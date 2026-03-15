int main(void) {
    // Q16.16: 2.0 * 3.0 = 6.0
    int a = 0x20000; // 2.0
    int b = 0x30000; // 3.0
    int r = __builtin_multfp(a, b);
    return (r >> 16) - 1; // expected=0x05
}

void interrupt(void) {}
