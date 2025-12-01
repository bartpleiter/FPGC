int main() {
    int a = -5;
    int i;
    for (i = -11; i <= 0; i++)
    {
        a = a + 1;
    }
    return a; // expected=0x07
}

void interrupt()
{

}
