// Test __fld with expression hi and constant lo.
// This exercises the codegen path where the hi argument is a non-constant
// expression and lo is a constant. Previously, GenPrep's commutative swap
// would incorrectly swap the operands, loading {0, expr} instead of {expr, 0}.

int global_val;

int main() {
    // Test 1: __fld with (constant - variable) as hi, 0 as lo
    global_val = 20;
    __fld(0, 120 - global_val, 0);  // Should load {100, 0} = 100.0
    int hi = __fsthi(0);
    // hi should be 100
    if (hi != 100)
        return 1;

    // Test 2: __fld with local variable as hi, 0 as lo
    int offset;
    offset = 42;
    __fld(1, offset, 0);  // Should load {42, 0} = 42.0
    hi = __fsthi(1);
    if (hi != 42)
        return 2;

    // Test 3: __fld with expression as lo, constant as hi (opposite case)
    global_val = 500;
    __fld(2, 0, global_val + 100);  // Should load {0, 600}
    int lo = __fstlo(2);
    if (lo != 600)
        return 3;

    // Test 4: verify full value with addition
    // Load 100.0 and 50.0, add them, check result = 150.0
    global_val = 50;
    __fld(3, 120 - 20, 0);          // {100, 0} = 100.0 (both constants)
    __fld(4, global_val, 0);         // {50, 0}  = 50.0  (variable hi)
    __fadd(5, 3, 4);                 // f5 = 150.0
    hi = __fsthi(5);
    if (hi != 150)
        return 4;

    return 150; // expected=0x96
}

void interrupt() {}
