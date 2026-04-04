/*
 * Register pressure stress test: 11 live locals with nested loops
 * and function calls — near the allocator limit (11 allocatable regs).
 * Tests that the spill pass correctly handles high register pressure.
 */

void interrupt(void) {}

int global_array[8] = {1, 2, 3, 4, 5, 6, 7, 8};

int fetch(int idx)
{
    return global_array[idx];
}

int compute(int a, int b, int c, int d)
{
    return a + b - c + d;
}

int main(void)
{
    int a, b, c, d, e, f, g, h;
    int sum, result, i;

    a = fetch(0);
    b = fetch(1);
    c = fetch(2);
    d = fetch(3);
    e = fetch(4);
    f = fetch(5);
    g = fetch(6);
    h = fetch(7);

    sum = a + b + c + d + e + f + g + h;
    result = 0;

    for (i = 0; i < 4; i++)
    {
        result = result + compute(a + i, b + i, c, d);
    }

    result = result + sum;

    /* compute(1+0, 2+0, 3, 4) = 1+2-3+4 = 4
     * compute(1+1, 2+1, 3, 4) = 2+3-3+4 = 6
     * compute(1+2, 2+2, 3, 4) = 3+4-3+4 = 8
     * compute(1+3, 2+3, 3, 4) = 4+5-3+4 = 10
     * result = 4+6+8+10 = 28
     * sum = 1+2+3+4+5+6+7+8 = 36
     * total = 28+36 = 64 = 0x40
     */
    return result; // expected=0x40
}
