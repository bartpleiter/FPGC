int main(void) {
    // Q16.16: 6.0 / 2.0 = 3.0
    int a = 0x60000; // 6.0
    int b = 0x20000; // 2.0
    int r = __builtin_divfp(a, b);
    return (r >> 16) + 4; // expected=0x07
}

void interrupt(void) {}
