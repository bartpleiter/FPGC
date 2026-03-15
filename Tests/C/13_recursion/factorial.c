int factorial(int n) {
    if (n <= 1) return 1;
    return n * factorial(n - 1);
}

int main(void) {
    // 5! = 120, we want 7. Use fib instead.
    // Actually: factorial(3) = 6, +1 = 7
    return factorial(3) + 1; // expected=0x07
}

void interrupt(void) {}
