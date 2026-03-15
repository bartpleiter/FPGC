int is_even(int n);
int is_odd(int n);

int is_even(int n)
{
    if (n == 0) return 1;
    return is_odd(n - 1);
}

int is_odd(int n)
{
    if (n == 0) return 0;
    return is_even(n - 1);
}

int main() {
    return is_even(14) + is_odd(13) + 5; // expected=0x07
}

void interrupt()
{

}
