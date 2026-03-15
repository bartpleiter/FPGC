int main(void) {
    char *s = "abcdefg";
    // 'g' is 103, minus 96 = 7
    return s[6] - 96; // expected=0x07
}

void interrupt(void) {}
