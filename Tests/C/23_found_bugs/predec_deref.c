// Test: pre-decrement through pointer --*ptr
// This tests that --*ptr returns the NEW (decremented) value, not the old value.
int main() {
    int x = 5;
    int *p = &x;
    int result = --*p;
    // x should be 4, result should be 4
    return result; // expected=0x04
}

void interrupt()
{

}
