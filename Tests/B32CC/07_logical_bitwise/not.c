int main() {
    int a = 0;
    int b = ~a;       // bitwise NOT of 0 = all 1s (-1 in signed)
    int c = ~b;       // bitwise NOT of -1 = 0
    
    int d = 5;
    int e = ~(~d);    // double NOT = identity
    
    return c + e + 2; // expected=0x07
}

void interrupt()
{

}
