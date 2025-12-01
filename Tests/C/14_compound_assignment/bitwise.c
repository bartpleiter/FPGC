int main() {
    int a = 15;
    a &= 7;   // a = 7 (0b1111 & 0b0111)
    a |= 8;   // a = 15 (0b0111 | 0b1000)
    a ^= 8;   // a = 7 (0b1111 ^ 0b1000)
    return a; // expected=0x07
}

void interrupt()
{

}
