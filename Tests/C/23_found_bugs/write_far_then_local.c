// Test: write to far address (via ptr), then write to local, then read local
// This tests the exact pattern that fails in --*ptr
int do_test(int *ptr) {
    *ptr = 4;        // write to far address (different cache line)
    int local = 4;   // write to local variable
    return local;    // read local back -> should be 4
}

int main() {
    int x = 5;
    int r = do_test(&x);
    return r; // expected=0x04
}

void interrupt()
{

}
