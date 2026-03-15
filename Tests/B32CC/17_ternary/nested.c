int main() {
    int a = 5;
    int b = 3;
    int c = 2;
    int d = (a > b) ? ((b > c) ? 7 : 1) : 0;
    return d; // expected=0x07
}

void interrupt()
{

}
