/*
 * Test: accessing global struct fields via pointers in a complex function.
 * This stresses the R12 scratch register path — the emit phase uses R12
 * for addr2reg when loading global addresses. Previously, if R12 held a
 * live value, it would be silently clobbered.
 */

void interrupt(void) {}

struct state {
    int field_a;
    int field_b;
    int field_c;
    int field_d;
    int field_e;
    int field_f;
};

struct state g_state;

int helper(int x) { return x + 1; }

int main(void)
{
    int a, b, c, d, e, f;
    int r1, r2, r3;
    int i, total;

    g_state.field_a = 10;
    g_state.field_b = 20;
    g_state.field_c = 30;
    g_state.field_d = 40;
    g_state.field_e = 50;
    g_state.field_f = 60;

    total = 0;

    for (i = 0; i < 3; i++)
    {
        /* Load all global fields — each needs addr2reg for the global address,
         * using R12 as scratch. All values must remain live for the
         * computation below. */
        a = g_state.field_a + i;
        b = g_state.field_b + i;
        c = g_state.field_c + i;
        d = g_state.field_d + i;
        e = g_state.field_e + i;
        f = g_state.field_f + i;

        /* Function calls between global accesses — forces spill/reload */
        r1 = helper(a + b);
        r2 = helper(c + d);
        r3 = helper(e + f);

        /* Use all values to prevent dead code elimination */
        total = total + r1 + r2 + r3 - a - b;
    }

    /* i=0: a=10,b=20,c=30,d=40,e=50,f=60
     *   r1=helper(30)=31, r2=helper(70)=71, r3=helper(110)=111
     *   total += 31+71+111-10-20 = 183
     * i=1: a=11,b=21,c=31,d=41,e=51,f=61
     *   r1=helper(32)=33, r2=helper(72)=73, r3=helper(112)=113
     *   total += 33+73+113-11-21 = 187
     * i=2: a=12,b=22,c=32,d=42,e=52,f=62
     *   r1=helper(34)=35, r2=helper(74)=75, r3=helper(114)=115
     *   total += 35+75+115-12-22 = 191
     * total = 183+187+191 = 561 = 0x231
     */
    return total & 0xFF; // expected=0x31
}
