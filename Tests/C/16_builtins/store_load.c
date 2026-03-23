/* Test __builtin_store and __builtin_load.
 * Write a value to a stack variable via its address, then read it back.
 * This verifies the builtins emit inline store/load instructions. */

int main(void) {
    int x = 0;
    int *p = &x;
    __builtin_store((int)p, 42);
    int r = __builtin_load((int)p);
    return r; // expected=0x2A
}

void interrupt(void) {}
