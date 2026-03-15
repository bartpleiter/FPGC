int main(void) {
    int a = 5;
    int b = 3;
    int result = 0;
    if (a > b) result += 1;
    if (a >= b) result += 1;
    if (b < a) result += 1;
    if (b <= a) result += 1;
    if (a == 5) result += 1;
    if (a != b) result += 2;
    return result; // expected=0x07
}

void interrupt(void) {}
