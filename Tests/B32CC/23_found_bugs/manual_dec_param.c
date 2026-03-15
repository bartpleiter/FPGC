// Test: manual decrement through function parameter pointer
// This does the same as --*ptr but step by step
int do_manual_dec(int *ptr) {
    int val = *ptr;
    val = val - 1;
    *ptr = val;
    return val;
}

int main() {
    int x = 5;
    int r = do_manual_dec(&x);
    return r; // expected=0x04
}

void interrupt()
{

}
