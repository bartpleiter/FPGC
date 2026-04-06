/*
 * Test: R12 clobber on store to global address.
 *
 * emit.c uses R12 as scratch in fixaddr() to load global symbol
 * addresses for store instructions (addr2reg $sym r12). If R12 is
 * also in the register allocator's pool, a live value in R12 gets
 * clobbered: fixaddr overwrites R12 with the address, then the
 * store instruction "write 0 r12 r12" stores the address instead
 * of the original value.
 *
 * This test creates 12 simultaneously live values (4 function params
 * in R4-R7, plus 8 computed values needing R1-R3 + R12 + R8-R11)
 * in a single basic block, then stores each computed value to a
 * separate global variable. With R12 in the allocatable pool, the
 * 4th computed value gets R12 and is corrupted when stored.
 *
 * Without fix: one global gets the symbol address instead of the
 *   computed value; the sum check fails.
 * With fix (R12 reserved): all values stored correctly.
 */

void interrupt(void) {}

int g1, g2, g3, g4, g5, g6, g7, g8;

int compute(int p1, int p2, int p3, int p4)
{
    int a = p1 + p2;    /* 30 */
    int b = p3 + p4;    /* 70 */
    int c = p1 + p3;    /* 40 */
    int d = p2 + p4;    /* 60 */
    int e = a + b;      /* 100 */
    int f = c + d;      /* 100 */
    int h = a + c;      /* 70 */
    int i = b + d;      /* 130 */

    /* 12 values live here (p1-p4 needed for return, a-i for stores).
     * Each store to a global triggers addr2reg into R12.
     * If any value is in R12, that store writes the address. */
    g1 = a;
    g2 = b;
    g3 = c;
    g4 = d;
    g5 = e;
    g6 = f;
    g7 = h;
    g8 = i;

    /* Keep p1-p4 live past the stores */
    return p1 + p2 + p3 + p4;
}

int main(void)
{
    int ret = compute(10, 20, 30, 40);
    /* ret = 100 */

    int sum = g1 + g2 + g3 + g4 + g5 + g6 + g7 + g8;
    /* 30 + 70 + 40 + 60 + 100 + 100 + 70 + 130 = 600 */

    return (ret + sum) & 0xFF; // expected=0xBC
    /* (100 + 600) & 0xFF = 700 & 0xFF = 0xBC */
}
