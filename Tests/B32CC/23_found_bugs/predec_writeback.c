// Test: check if --*ptr modifies the pointed-to value
// Return x directly to see if the write-back through pointer worked
int do_predec(int *ptr) {
    --*ptr;
    return 0; // dummy return
}

int main() {
    int x = 5;
    do_predec(&x);
    return x; // expected=0x04 if write-back worked, 0x05 if not
}

void interrupt()
{

}
