// Test __fsthi/__fstlo in pointer write context
// This tests the gotUnary handling for assignment through pointers

void fp64_add(int a_hi, int a_lo,
              int *out_hi, int *out_lo)
{
    __fld(6, a_hi, a_lo);
    *out_hi = __fsthi(6);
    *out_lo = __fstlo(6);
}

int main() {
    int hi;
    int lo;
    fp64_add(5, 3, &hi, &lo);
    return hi * 10 + lo; // expected=0x35
}

void interrupt() {}
