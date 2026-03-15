int main(void) {
    char buf[8];
    buf[0] = 'H';
    buf[1] = 'e';
    buf[2] = 'l';
    buf[3] = 'l';
    buf[4] = 'o';
    // Test char array: 'o' is 111, minus 104 = 7
    return buf[4] - 104; // expected=0x07
}

void interrupt(void) {}
