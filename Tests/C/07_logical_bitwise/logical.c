int main(void) {
    int a = 1;
    int b = 0;
    int result = 0;

    if (a && !b) result += 3;  // true
    if (a || b) result += 2;   // true
    if (!(a && b)) result += 2; // true (a&&b is false)
    return result; // expected=0x07
}

void interrupt(void) {}
