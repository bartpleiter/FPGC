int main() {
    int a = 1;
    a <<= 3;  // a = 8
    a >>= 1;  // a = 4
    a <<= 1;  // a = 8
    a -= 1;   // a = 7
    return a; // expected=0x07
}

void interrupt()
{

}
