// Tests passing more than 4 args (r4-r7 are arg regs, rest go on stack)
int sum6(int a, int b, int c, int d, int e, int f) {
    return a + b + c + d + e + f;
}

int main(void) {
    return sum6(1, 1, 1, 1, 1, 2); // expected=0x07
}

void interrupt(void) {}
