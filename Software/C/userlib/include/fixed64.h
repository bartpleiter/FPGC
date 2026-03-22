#ifndef FIXED64_H
#define FIXED64_H

/*
 * fixed64 — Q32.32 signed fixed-point arithmetic library.
 *
 * Uses the FP64 hardware coprocessor for add, subtract, and multiply.
 * Software fallback for division (Newton-Raphson).
 *
 * Value = hi + lo / 2^32
 *
 * All functions write results through pointer parameters to avoid
 * QBE B32P3 backend limitations with aggregate return types.
 */

struct fp64
{
    int hi;
    unsigned int lo;
};

/* Construction */
void fp64_make(struct fp64 *out, int hi, unsigned int lo);
void fp64_from_int(struct fp64 *out, int val);
void fp64_from_fp16(struct fp64 *out, int fp16_val);

/* Arithmetic (uses FP64 coprocessor) */
void fp64_add(struct fp64 *out, struct fp64 *a, struct fp64 *b);
void fp64_sub(struct fp64 *out, struct fp64 *a, struct fp64 *b);
void fp64_mul(struct fp64 *out, struct fp64 *a, struct fp64 *b);
void fp64_neg(struct fp64 *out, struct fp64 *a);
void fp64_abs(struct fp64 *out, struct fp64 *a);

/* Comparison: negative if a < b, 0 if equal, positive if a > b */
int fp64_cmp(struct fp64 *a, struct fp64 *b);

/* Conversion */
int fp64_to_int(struct fp64 *val);
int fp64_to_fp16(struct fp64 *val);

/* Shift operations */
void fp64_shr(struct fp64 *out, struct fp64 *val, int n);
void fp64_shl(struct fp64 *out, struct fp64 *val, int n);

/* Division (software Newton-Raphson) */
void fp64_div(struct fp64 *out, struct fp64 *a, struct fp64 *b);

/* Inline-style initializer macro (for local stack variables) */
#define FP64_SET(v, h, l)  do { (v).hi = (h); (v).lo = (l); } while(0)
#define FP64_ZERO_INIT     { 0, 0 }
#define FP64_ONE_INIT      { 1, 0 }

/*
 * Low-level FP64 coprocessor functions (implemented in fixed64_asm.asm).
 * fp6 and fp7 are used as temporaries by the library.
 */
void fp64_hw_load6(int hi, int lo);
void fp64_hw_load7(int hi, int lo);
int  fp64_hw_store_hi6(void);
int  fp64_hw_store_lo6(void);
void fp64_hw_add66_7(void);
void fp64_hw_sub66_7(void);
void fp64_hw_mul66_7(void);

#endif /* FIXED64_H */
