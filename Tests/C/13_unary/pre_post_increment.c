int main() {
    int a = 5;
    int b = ++a;  // pre-increment: a becomes 6, b gets 6
    int c = a++;  // post-increment: c gets 6, a becomes 7
    return c + a; // expected=0x0D
}

void interrupt()
{

}
