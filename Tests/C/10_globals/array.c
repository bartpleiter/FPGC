int values[4] = {10, 20, 30, 40};

int main(void) {
    // 30 + 40 - 63 = 7
    return values[2] + values[3] - 63; // expected=0x07
}

void interrupt(void) {}
