int main(void) {
    char c = 7;
    int x = (int)c;
    return x; // expected=0x07
}

void interrupt(void) {}
