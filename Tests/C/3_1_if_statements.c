int main() {
    int a = 2;
    if (a < 2)
    {
        a = 5;
    }
    else if (a == 2)
    {
        a = 3;
    }
    else
    {
        a = 4;
    }
    return a; // expected=0x03
}

void interrupt()
{

}
