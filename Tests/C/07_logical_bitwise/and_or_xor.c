int main() {
    int a = 60;       // 0011 1100
    int b = 13;       // 0000 1101
    
    int c = a & b;    // 0000 1100 = 12 (bitwise AND)
    int d = a | b;    // 0011 1101 = 61 (bitwise OR)
    int e = a ^ b;    // 0011 0001 = 49 (bitwise XOR)
    
    return c + d + e - 115; // expected=0x07
}

void interrupt()
{

}
