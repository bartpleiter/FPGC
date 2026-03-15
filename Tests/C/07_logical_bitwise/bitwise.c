int main(void) {
    int a = 0x0F;
    int b = 0x37;
    // a & b = 0x07
    return a & b; // expected=0x07
}

void interrupt(void) {}
