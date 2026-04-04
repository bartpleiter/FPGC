/*
 * Test: 8+ values live across a function call.
 * With NGPR=15 and nrsave=8, the buggy spill formula
 * allows 15-8=7 temps across calls. But only 4 callee-save
 * regs (R8-R11) actually survive. This test forces 8 non-constant
 * values to be live across a call, exceeding the buggy limit.
 *
 * All values are loaded from a global array AND modified each
 * iteration to prevent constant folding.
 */

void interrupt(void) {}

int vals[8] = {1, 2, 3, 4, 5, 6, 7, 8};
int sink;

/* Takes 4 args to consume registers, returns 0 */
int touch4(int w, int x, int y, int z)
{
    sink = w + x + y + z;
    return 0;
}

int main(void)
{
    int a, b, c, d, e, f, g, h;
    int i, sum;

    a = vals[0]; b = vals[1]; c = vals[2]; d = vals[3];
    e = vals[4]; f = vals[5]; g = vals[6]; h = vals[7];
    sum = 0;

    for (i = 0; i < 3; i++)
    {
        /* touch4 uses 4 arg registers; a-h + i + sum = 10
         * values must survive this call */
        touch4(a, b, c, d);

        sum = sum + a + b + c + d + e + f + g + h;
        a = a + 1; b = b + 1; c = c + 1; d = d + 1;
        e = e + 1; f = f + 1; g = g + 1; h = h + 1;
    }

    /* Iter 0: sum += 1+2+3+4+5+6+7+8 = 36
     * Iter 1: sum += 2+3+4+5+6+7+8+9 = 44
     * Iter 2: sum += 3+4+5+6+7+8+9+10 = 52
     * Total = 36+44+52 = 132 = 0x84
     */
    return sum; // expected=0x84
}
