int main() {
    // Multiple returns in same function
    int x = 3;
    if (x < 0)
        return 1;
    if (x == 0)
        return 2;
    if (x > 5)
        return 3;
    return 7; // expected=0x07
}

void interrupt()
{

}
