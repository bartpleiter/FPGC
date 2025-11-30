int n()
{
    return 5;
}

int main() {
    int a = 3;
    int b = n();
    int c = a + b;
    return c; // expected=0x08
}

void interrupt()
{

}
