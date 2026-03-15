int main() {
    int a = 0;
    int b = 0;

    if (a > b)
        return 1;
    if (a < b)
        return 2;
    if (a != b)
        return 3;

    int c = 7;
    int d = 7;

    if (c > d)
        return 4;
    if (c < d)
        return 5;
    if (c != d)
        return 6;

    int x = 3;
    int y = 5;

    if (x == y)
        return 1;
    if (x >= y)
        return 2;
    if (y <= x)
        return 3;
    if (x > y)
        return 4;
    if (y < x)
        return 5;

    return 99; // expected=0x63
}

void interrupt()
{

}
