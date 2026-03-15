int double_it(int x) {
    return x * 2;
}

int add_one(int x) {
    return x + 1;
}

int main(void) {
    return add_one(double_it(3)); // expected=0x07
}

void interrupt(void) {}
