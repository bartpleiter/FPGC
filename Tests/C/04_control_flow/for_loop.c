int main(void) {
    int sum = 0;
    for (int i = 1; i <= 4; i++) {
        sum += i;
    }
    // 1+2+3+4 = 10, minus 3
    return sum - 3; // expected=0x07
}

void interrupt(void) {}
