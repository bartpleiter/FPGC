int sum(int *arr, int n) {
    int total = 0;
    for (int i = 0; i < n; i++) {
        total += arr[i];
    }
    return total;
}

int main(void) {
    int data[3];
    data[0] = 2;
    data[1] = 2;
    data[2] = 3;
    return sum(data, 3); // expected=0x07
}

void interrupt(void) {}
