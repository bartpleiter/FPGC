int main(void) {
    int arr[4];
    arr[0] = 2;
    arr[1] = 5;
    arr[2] = 8;
    arr[3] = 11;

    int *p = arr;
    // Pointer arithmetic: p+2 points to arr[2]
    return *(p + 2) - 1; // expected=0x07
}

void interrupt(void) {}
