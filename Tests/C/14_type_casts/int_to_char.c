int main(void) {
    int x = 263;
    char c = (char)x;
    // 263 % 256 = 7
    return c; // expected=0x07
}

void interrupt(void) {}
