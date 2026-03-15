int main(void) {
    int n = 10;
    int count = 0;
    while (n > 0) {
        n -= 3;
        count++;
    }
    // n goes 10->7->4->1->(-2), count=4, then 4+3=7
    return count + 3; // expected=0x07
}

void interrupt(void) {}
