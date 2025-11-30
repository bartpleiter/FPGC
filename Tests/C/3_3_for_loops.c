int main() {
    int a = 5;
    int i;
    for (i = 0; i < 10; i++)
    {
        a = a + 1;
    }
    return a; // expected=0x0F
}

void interrupt()
{

}
