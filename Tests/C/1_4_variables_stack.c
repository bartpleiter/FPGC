int main() {
    int a = 1;
    int b = 2;
    int c = a + b;
    int d = c + a;
    int e = d + b + 1;
    return e + 2; // expected=0x09
}

void interrupt()
{

}
