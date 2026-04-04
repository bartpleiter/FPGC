/*
 * Test: many values live across a call in nested loop.
 * 6 values must survive a function call inside a loop.
 * This exceeds 4 callee-save registers, requiring the
 * spill pass to correctly spill some values.
 */

void interrupt(void) {}

int global_acc;

int accumulate(int x)
{
    global_acc = global_acc + x;
    return global_acc;
}

int main(void)
{
    int a, b, c, d, e, f;
    int i, total;

    a = 1;
    b = 2;
    c = 3;
    d = 4;
    e = 5;
    f = 6;
    total = 0;
    global_acc = 0;

    for (i = 0; i < 5; i++)
    {
        /* 6 values (a-f) + i + total = 8 values live across call */
        accumulate(i);

        /* Use all values after the call */
        total = total + a + b + c + d + e + f;
        a = a + 1;
    }

    /* a: 1,2,3,4,5 across iterations
     * iter 0: total = 0 + 1+2+3+4+5+6 = 21
     * iter 1: total = 21 + 2+2+3+4+5+6 = 43
     * iter 2: total = 43 + 3+2+3+4+5+6 = 66
     * iter 3: total = 66 + 4+2+3+4+5+6 = 90
     * iter 4: total = 90 + 5+2+3+4+5+6 = 115 = 0x73
     */
    return total; // expected=0x73
}
