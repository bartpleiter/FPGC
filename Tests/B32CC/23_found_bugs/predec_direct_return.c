// Test: return --*ptr directly, no local variable
int do_predec(int *ptr) {
    return --*ptr;  // return directly
}

int main() {
    int x = 5;
    int r = do_predec(&x);
    return r; // expected=0x04
}

void interrupt()
{

}
