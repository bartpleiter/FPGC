int fib(int n) {
    if (n <= 0) return 0;
    if (n == 1) return 1;
    return fib(n - 1) + fib(n - 2);
}

int main(void) {
    // fib(5) = 5, fib(4) = 3, fib(6) = 8
    // fib(5) + 2 = 7
    return fib(5) + 2; // expected=0x07
}

void interrupt(void) {}
