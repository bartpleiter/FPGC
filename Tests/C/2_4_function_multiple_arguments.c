int n(int x, int y, int z)
{
    return x + y + z;
}

int main() {
    int a = 1;
    int b = 2;
    int c = 3;
    int d = n(a, b, c);
    return d; // expected=0x06
}

void interrupt()
{

}
