int main() {
    int a = 1;
    
    int b = a << 3;   // 1 << 3 = 8
    int c = b >> 1;   // 8 >> 1 = 4
    int d = c << 1;   // 4 << 1 = 8
    int e = d >> 3;   // 8 >> 3 = 1
    
    return b + c + d - e - 12; // expected=0x07
}

void interrupt()
{

}
