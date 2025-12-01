int main() {
    int i = 10;
    int x = 0;
    while (i > 3)
    {
        i = i - 1;
        x = x + 1;
    }
    return x; // expected=0x07
}

void interrupt()
{

}
