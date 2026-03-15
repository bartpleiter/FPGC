int counter = 3;

int main(void) {
    counter = counter + 4;
    return counter; // expected=0x07
}

void interrupt(void) {}
