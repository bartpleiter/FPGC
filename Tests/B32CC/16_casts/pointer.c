int main() {
    int a = 100;
    int *p = (int *)a;  // cast int to pointer
    int b = (int)p;     // cast pointer back to int
    return b - 93; // expected=0x07
}

void interrupt()
{

}
