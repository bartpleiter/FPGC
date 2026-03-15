int main(void) {
    int a = 5;
    if (a > 3) {
        return 7; // expected=0x07
    }
    return 0;
}

void interrupt(void) {}
