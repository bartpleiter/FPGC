// Test 2D char array access
unsigned char arr[3][4];

int main() {
    // Write to various positions
    arr[0][0] = 1;
    arr[0][1] = 2;
    arr[0][2] = 3;
    arr[0][3] = 4;
    arr[1][0] = 5;
    arr[1][1] = 6;
    arr[1][2] = 7;
    arr[1][3] = 8;
    arr[2][0] = 9;
    arr[2][1] = 10;
    arr[2][2] = 11;
    arr[2][3] = 12;
    
    // Read back and verify
    // Sum should be 1+2+3+4+5+6+7+8+9+10+11+12 = 78
    int sum = arr[0][0] + arr[0][1] + arr[0][2] + arr[0][3];
    sum = sum + arr[1][0] + arr[1][1] + arr[1][2] + arr[1][3];
    sum = sum + arr[2][0] + arr[2][1] + arr[2][2] + arr[2][3];
    
    return sum - 71; // expected=0x07
}

void interrupt() {
}
