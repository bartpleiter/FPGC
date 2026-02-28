// Test: write distinct values to far and local, read local back
// This tells us exactly which value is being read
int do_test(int *p) {
    int a;
    *p = 10;     // write 10 to far address (via pointer)
    a = 20;      // write 20 to local
    return a;    // read local -> should be 20
}

int main() {
    int x = 5;
    int r = do_test(&x);
    return r; // expected=0x14
}

void interrupt()
{

}
