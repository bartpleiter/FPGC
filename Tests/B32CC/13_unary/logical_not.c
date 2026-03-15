int main() {
    int a = 0;
    int b = 5;
    int c = 0;
    
    if (!a)
        c = c + 3;
    if (!b)
        c = c + 100; // should not happen
    
    return c + 4; // expected=0x07
}

void interrupt()
{

}
