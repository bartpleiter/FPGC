/*
 * fixed64.c — Q32.32 signed fixed-point arithmetic library.
 *
 * Uses the FP64 hardware coprocessor (via assembly helpers in fixed64_asm.asm)
 * for add, subtract, and multiply. Software fallback for division.
 *
 * FP64 register allocation:
 *   fp6, fp7 are used as temporaries by library functions.
 *   User code should use fp0-fp5 freely.
 *
 * All results written through pointer parameters to avoid QBE B32P3
 * backend crashes with aggregate return types in complex control flow.
 */

#include <fixed64.h>

/* Assembly helpers (always use fp6/fp7) */
extern void fp64_hw_load6(int hi, int lo);
extern void fp64_hw_load7(int hi, int lo);
extern int  fp64_hw_store_hi6(void);
extern int  fp64_hw_store_lo6(void);
extern void fp64_hw_add66_7(void);
extern void fp64_hw_sub66_7(void);
extern void fp64_hw_mul66_7(void);

/* ---- Construction ---- */

void fp64_make(struct fp64 *out, int hi, unsigned int lo)
{
    out->hi = hi;
    out->lo = lo;
}

void fp64_from_int(struct fp64 *out, int val)
{
    out->hi = val;
    out->lo = 0;
}

void fp64_from_fp16(struct fp64 *out, int fp16_val)
{
    out->hi = fp16_val >> 16;
    out->lo = (unsigned int)(fp16_val & 0xFFFF) << 16;
}

/* ---- Arithmetic ---- */

void fp64_add(struct fp64 *out, struct fp64 *a, struct fp64 *b)
{
    fp64_hw_load6(a->hi, a->lo);
    fp64_hw_load7(b->hi, b->lo);
    fp64_hw_add66_7();
    out->hi = fp64_hw_store_hi6();
    out->lo = fp64_hw_store_lo6();
}

void fp64_sub(struct fp64 *out, struct fp64 *a, struct fp64 *b)
{
    fp64_hw_load6(a->hi, a->lo);
    fp64_hw_load7(b->hi, b->lo);
    fp64_hw_sub66_7();
    out->hi = fp64_hw_store_hi6();
    out->lo = fp64_hw_store_lo6();
}

void fp64_mul(struct fp64 *out, struct fp64 *a, struct fp64 *b)
{
    fp64_hw_load6(a->hi, a->lo);
    fp64_hw_load7(b->hi, b->lo);
    fp64_hw_mul66_7();
    out->hi = fp64_hw_store_hi6();
    out->lo = fp64_hw_store_lo6();
}

void fp64_neg(struct fp64 *out, struct fp64 *a)
{
    fp64_hw_load6(0, 0);
    fp64_hw_load7(a->hi, a->lo);
    fp64_hw_sub66_7();
    out->hi = fp64_hw_store_hi6();
    out->lo = fp64_hw_store_lo6();
}

void fp64_abs(struct fp64 *out, struct fp64 *a)
{
    if (a->hi < 0)
    {
        fp64_neg(out, a);
    }
    else
    {
        out->hi = a->hi;
        out->lo = a->lo;
    }
}

/* ---- Comparison ---- */

int fp64_cmp(struct fp64 *a, struct fp64 *b)
{
    if (a->hi != b->hi)
        return a->hi - b->hi;
    if (a->lo == b->lo)
        return 0;
    return (a->lo > b->lo) ? 1 : -1;
}

/* ---- Conversion ---- */

int fp64_to_int(struct fp64 *val)
{
    return val->hi;
}

int fp64_to_fp16(struct fp64 *val)
{
    return (val->hi << 16) | (val->lo >> 16);
}

/* ---- Shift operations ---- */

void fp64_shr(struct fp64 *out, struct fp64 *val, int n)
{
    if (n <= 0)
    {
        out->hi = val->hi;
        out->lo = val->lo;
        return;
    }
    if (n >= 64)
    {
        out->hi = (val->hi < 0) ? -1 : 0;
        out->lo = 0;
        return;
    }
    if (n >= 32)
    {
        out->lo = (unsigned int)(val->hi >> (n - 32));
        out->hi = (val->hi < 0) ? -1 : 0;
        return;
    }
    out->lo = (val->lo >> n) | ((unsigned int)val->hi << (32 - n));
    out->hi = val->hi >> n;
}

void fp64_shl(struct fp64 *out, struct fp64 *val, int n)
{
    if (n <= 0)
    {
        out->hi = val->hi;
        out->lo = val->lo;
        return;
    }
    if (n >= 64)
    {
        out->hi = 0;
        out->lo = 0;
        return;
    }
    if (n >= 32)
    {
        out->hi = (int)(val->lo << (n - 32));
        out->lo = 0;
        return;
    }
    out->hi = (val->hi << n) | (int)(val->lo >> (32 - n));
    out->lo = val->lo << n;
}

/* ---- Division (Newton-Raphson) ---- */

void fp64_div(struct fp64 *out, struct fp64 *a, struct fp64 *b)
{
    struct fp64 x, two, bx, correction, aa, bb;
    int neg_a;
    int neg_b;
    int i;

    neg_a = 0;
    neg_b = 0;

    aa.hi = a->hi;
    aa.lo = a->lo;
    bb.hi = b->hi;
    bb.lo = b->lo;

    if (aa.hi < 0)
    {
        fp64_neg(&aa, &aa);
        neg_a = 1;
    }
    if (bb.hi < 0)
    {
        fp64_neg(&bb, &bb);
        neg_b = 1;
    }

    if (bb.hi == 0 && bb.lo == 0)
    {
        if (neg_a != neg_b)
        {
            out->hi = -2147483647;
            out->lo = 0;
        }
        else
        {
            out->hi = 2147483647;
            out->lo = 0xFFFFFFFF;
        }
        return;
    }

    /* Initial estimate of 1/b */
    if (bb.hi > 0)
    {
        int shift;
        int bhi;
        shift = 0;
        bhi = bb.hi;
        while (bhi > 1)
        {
            bhi >>= 1;
            shift++;
        }
        if (shift < 31)
        {
            x.hi = 0;
            x.lo = 0x80000000u >> shift;
        }
        else
        {
            x.hi = 0;
            x.lo = 1;
        }
    }
    else
    {
        int shift;
        unsigned int blo;
        shift = 0;
        blo = bb.lo;
        while (blo != 0 && (blo & 0x80000000u) == 0)
        {
            blo <<= 1;
            shift++;
        }
        if ((shift + 1) > 30)
        {
            x.hi = 0x40000000;
            x.lo = 0;
        }
        else
        {
            x.hi = 1 << (shift + 1);
            x.lo = 0;
        }
    }

    /* Newton-Raphson: x = x * (2 - b * x) */
    two.hi = 2;
    two.lo = 0;
    for (i = 0; i < 5; i++)
    {
        fp64_mul(&bx, &bb, &x);
        fp64_sub(&correction, &two, &bx);
        fp64_mul(&x, &x, &correction);
    }

    fp64_mul(&x, &aa, &x);

    if (neg_a != neg_b)
    {
        fp64_neg(&x, &x);
    }

    out->hi = x.hi;
    out->lo = x.lo;
}
