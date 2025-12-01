int main() {
    int a = 5;
    int b = 3;
    // Test that && has lower precedence than comparison
    int c = a > b && b > 1;  // (a > b) && (b > 1) = 1 && 1 = 1
    // Test that || has lower precedence than &&
    int d = 0 || a > b && 1;  // 0 || ((a > b) && 1) = 0 || 1 = 1
    return c + d + 5; // expected=0x07
}

void interrupt()
{

}
