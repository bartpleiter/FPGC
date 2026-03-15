int main(void) {
    int a = 10;
    int *p = &a;
    *p = 7;
    return a; // expected=0x07
}

void interrupt(void) {}
