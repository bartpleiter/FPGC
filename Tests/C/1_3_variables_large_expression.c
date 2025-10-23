int main() {
    int a = 1;
    int b = 2;
    int c = 3;
    int d = 4;
    
    // Complex nested expression requiring multiple temps
    int result = (a + b) * (c + d) + (a - b) * (c - d);
    return result; // expected=0x16
}

void interrupt()
{

}
