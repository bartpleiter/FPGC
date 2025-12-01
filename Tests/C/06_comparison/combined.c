int main() {
    int a = 5;
    int b = 3;
    int c = 0;
    
    // Test less than
    if (b < a)
        c = c + 1;
    
    // Test greater than
    if (a > b)
        c = c + 2;
    
    // Test less than or equal
    if (b <= a)
        c = c + 4;
    
    // Test greater than or equal
    if (a >= b)
        c = c + 8;
    
    return c; // expected=0x0F
}

void interrupt()
{

}
