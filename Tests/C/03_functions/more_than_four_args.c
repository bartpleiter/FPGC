// Four registers are used for passing arguments, so we want to test using more than four.
int n(int a, int b, int c, int d, int e, int f)
{
    return a + b + c + d + e + f;
}

int main() {
    int a = 1;
    int b = 2;
    int c = 3;
    int d = 4;
    int e = 5;
    int f = 6;
    int result = n(a, b, c, d, e, f);
    return result; // expected=0x15
}

void interrupt()
{

}
