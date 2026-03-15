void set_value(int *p, int val)
{
    *p = val;
}

int main() {
    int a = 0;
    set_value(&a, 7);
    return a; // expected=0x07
}

void interrupt()
{

}
