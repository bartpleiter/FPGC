// Test: pre-decrement through function parameter pointer
// This specifically tests --*ptr where ptr is a function parameter
int do_predec(int *ptr) {
    int result = --*ptr;
    return result;
}

int main() {
    int x = 5;
    int r = do_predec(&x);
    // If correct: x=4, r=4, return 4
    // If --*ptr returns old value: x=4, r=5, return 5
    return r; // expected=0x04
}

void interrupt()
{

}
