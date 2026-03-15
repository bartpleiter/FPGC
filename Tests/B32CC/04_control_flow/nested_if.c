int main() {
    int a = 3;
    int b = 5;
    int c = 0;
    
    if (a > 0)
    {
        if (b > a)
        {
            c = 7;
        }
        else
        {
            c = 1;
        }
    }
    else
    {
        c = 2;
    }
    
    return c; // expected=0x07
}

void interrupt()
{

}
