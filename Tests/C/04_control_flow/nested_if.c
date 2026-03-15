int main(void) {
    int x = 2;
    int result;
    if (x == 1) {
        result = 10;
    } else if (x == 2) {
        result = 7;
    } else {
        result = 0;
    }
    return result; // expected=0x07
}

void interrupt(void) {}
