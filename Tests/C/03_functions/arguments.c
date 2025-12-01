int n(int x)
{
    return x + 2;
}

int main() {
    int a = 3;
    int b = n(a);
    int c = a + b; //3+5=8
    return c; // expected=0x08
}

void interrupt()
{

}
