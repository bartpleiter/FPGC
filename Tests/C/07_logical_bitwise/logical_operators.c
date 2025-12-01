int main() {
    int a = 1;
    int b = 0;
    int c = 0;
    
    // Test logical AND
    if (a && a)
        c = c + 1;
    if (a && b)
        c = c + 100; // should not happen
    
    // Test logical OR
    if (a || b)
        c = c + 2;
    if (b || b)
        c = c + 100; // should not happen
    
    // Test logical NOT
    if (!b)
        c = c + 4;
    if (!a)
        c = c + 100; // should not happen
    
    return c; // expected=0x07
}

void interrupt()
{

}
