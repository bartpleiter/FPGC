int main(void) {
    int a = 1;
    int b = 2;
    int c = 3;
    int d = 4;
    int e = 5;
    // Forces register allocation across many locals
    return (a + b + c + d + e) - 8; // expected=0x07
}

void interrupt(void) {}
