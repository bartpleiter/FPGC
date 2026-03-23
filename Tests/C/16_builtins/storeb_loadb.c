/* Test __builtin_storeb and __builtin_loadb (byte-level store/load).
 * Write a byte value and read it back. */

int main(void) {
    int buf = 0;
    int *p = &buf;
    __builtin_storeb((int)p, 0xAB);
    int r = __builtin_loadb((int)p);
    return r; // expected=0xAB
}

void interrupt(void) {}
