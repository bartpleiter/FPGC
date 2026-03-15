int main(void) {
    int x = 10;
    x = x + 5;
    x = x - 8;
    return x; // expected=0x07
}

void interrupt(void) {}
