int main(void) {
    int sum = 0;
    for (int i = 0; i < 10; i++) {
        if (i == 5) break;
        sum += i;
    }
    // 0+1+2+3+4 = 10, minus 3
    return sum - 3; // expected=0x07
}

void interrupt(void) {}
