int main() {
    int a = 1;
    int b = 2;
    int c = 3;
    int d = a + b + c + (a * b) + (b * c) - (a + c) + 2;
    // 1 + 2 + 3 + 2 + 6 - 4 + 2 = 12
    return d - 5; // expected=0x07
}

void interrupt()
{

}
