//
// fixed64 — Q32.32 signed fixed-point arithmetic library.
//
// Uses the FP64 hardware coprocessor for add, subtract, and multiply.
// Software fallback for division (Newton-Raphson).
//
// FP64 register allocation for library functions:
//   f6, f7 are used as temporaries by library functions.
//   User code should use f0-f5 freely. If using library functions,
//   avoid f6-f7 as they may be clobbered.
//

#include "libs/common/fixed64.h"

// ---- Construction ----

struct fp64 fp64_make(int hi, unsigned int lo)
{
  struct fp64 r;
  r.hi = hi;
  r.lo = lo;
  return r;
}

struct fp64 fp64_from_int(int val)
{
  struct fp64 r;
  r.hi = val;
  r.lo = 0;
  return r;
}

struct fp64 fp64_from_fp16(int fp16_val)
{
  struct fp64 r;
  r.hi = fp16_val >> 16;
  // Low 16 bits of fp16 are the fractional part (in 0.16 format).
  // Shift left by 16 to convert to 0.32 format.
  r.lo = (unsigned int)(fp16_val & 0xFFFF) << 16;
  return r;
}

// ---- Arithmetic ----

struct fp64 fp64_add(struct fp64 a, struct fp64 b)
{
  struct fp64 r;
  __fld(6, a.hi, a.lo);
  __fld(7, b.hi, b.lo);
  __fadd(6, 6, 7);
  r.hi = __fsthi(6);
  r.lo = __fstlo(6);
  return r;
}

struct fp64 fp64_sub(struct fp64 a, struct fp64 b)
{
  struct fp64 r;
  __fld(6, a.hi, a.lo);
  __fld(7, b.hi, b.lo);
  __fsub(6, 6, 7);
  r.hi = __fsthi(6);
  r.lo = __fstlo(6);
  return r;
}

struct fp64 fp64_mul(struct fp64 a, struct fp64 b)
{
  struct fp64 r;
  __fld(6, a.hi, a.lo);
  __fld(7, b.hi, b.lo);
  __fmul(6, 6, 7);
  r.hi = __fsthi(6);
  r.lo = __fstlo(6);
  return r;
}

struct fp64 fp64_neg(struct fp64 a)
{
  struct fp64 r;
  // -a = 0 - a
  __fld(6, 0, 0);
  __fld(7, a.hi, a.lo);
  __fsub(6, 6, 7);
  r.hi = __fsthi(6);
  r.lo = __fstlo(6);
  return r;
}

struct fp64 fp64_abs(struct fp64 a)
{
  if (a.hi < 0 || (a.hi == 0 && a.lo == 0))
  {
    if (a.hi < 0)
    {
      return fp64_neg(a);
    }
  }
  return a;
}

// ---- Comparison ----

int fp64_cmp(struct fp64 a, struct fp64 b)
{
  if (a.hi != b.hi)
  {
    return a.hi - b.hi;
  }
  if (a.lo == b.lo)
  {
    return 0;
  }
  // Unsigned comparison of fractional parts
  if (a.lo > b.lo)
  {
    return 1;
  }
  return -1;
}

// ---- Conversion ----

int fp64_to_int(struct fp64 val)
{
  return val.hi;
}

int fp64_to_fp16(struct fp64 val)
{
  // hi goes to bits 31:16, top 16 of lo goes to bits 15:0
  return (val.hi << 16) | (val.lo >> 16);
}

// ---- Shift operations ----

struct fp64 fp64_shr(struct fp64 val, int n)
{
  struct fp64 r;
  if (n <= 0)
  {
    return val;
  }
  if (n >= 64)
  {
    r.hi = val.hi < 0 ? -1 : 0;
    r.lo = 0;
    return r;
  }
  if (n >= 32)
  {
    // Shift by 32+: lo gets bits from hi, hi gets sign extension
    r.lo = (unsigned int)(val.hi >> (n - 32));
    r.hi = val.hi < 0 ? -1 : 0;
    return r;
  }
  // n is 1..31
  r.lo = (val.lo >> n) | ((unsigned int)val.hi << (32 - n));
  r.hi = val.hi >> n;
  return r;
}

struct fp64 fp64_shl(struct fp64 val, int n)
{
  struct fp64 r;
  if (n <= 0)
  {
    return val;
  }
  if (n >= 64)
  {
    r.hi = 0;
    r.lo = 0;
    return r;
  }
  if (n >= 32)
  {
    r.hi = (int)(val.lo << (n - 32));
    r.lo = 0;
    return r;
  }
  // n is 1..31
  r.hi = (val.hi << n) | (int)(val.lo >> (32 - n));
  r.lo = val.lo << n;
  return r;
}

// ---- Division (Newton-Raphson) ----
//
// Computes a / b using iterative reciprocal refinement:
//   1. Estimate x0 ≈ 1/b using integer leading-bit heuristic
//   2. Refine: x_{n+1} = x_n * (2 - b * x_n)   (4 iterations)
//   3. Result = a * x_final
//
// This avoids any hardware division.

struct fp64 fp64_div(struct fp64 a, struct fp64 b)
{
  struct fp64 x;
  struct fp64 two;
  struct fp64 bx;
  struct fp64 correction;
  int neg_a;
  int neg_b;
  int i;

  // Handle signs: work with positive values, fix sign at end
  neg_a = 0;
  neg_b = 0;
  if (a.hi < 0)
  {
    a = fp64_neg(a);
    neg_a = 1;
  }
  if (b.hi < 0)
  {
    b = fp64_neg(b);
    neg_b = 1;
  }

  // Handle trivial cases
  if (b.hi == 0 && b.lo == 0)
  {
    // Division by zero: return max positive or negative value
    if (neg_a != neg_b)
    {
      return fp64_make(-2147483647, 0);
    }
    return fp64_make(2147483647, 0xFFFFFFFF);
  }

  // Initial estimate of 1/b.
  // Find approximate magnitude of b, then set x0 = 2^(32-log2(b)).
  // For b in [1, 2): x0 ≈ 1.0
  // For b in [2, 4): x0 ≈ 0.5
  // etc.
  if (b.hi > 0)
  {
    // b >= 1.0: reciprocal < 1.0
    // Count leading zeros of b.hi to estimate magnitude
    int shift;
    int bhi;
    shift = 0;
    bhi = b.hi;
    while (bhi > 1)
    {
      bhi = bhi >> 1;
      shift = shift + 1;
    }
    // b ≈ 2^shift, so 1/b ≈ 2^(-shift) = {hi=0, lo=2^(31-shift)}
    if (shift < 31)
    {
      x = fp64_make(0, 0x80000000u >> shift);
    }
    else
    {
      x = fp64_make(0, 1);
    }
  }
  else
  {
    // b < 1.0 (b.hi == 0, b.lo > 0): reciprocal > 1.0
    int shift;
    unsigned int blo;
    shift = 0;
    blo = b.lo;
    while (blo != 0 && (blo & 0x80000000u) == 0)
    {
      blo = blo << 1;
      shift = shift + 1;
    }
    // b ≈ 2^(-(shift+1)), so 1/b ≈ 2^(shift+1)
    x = fp64_make(1 << (shift + 1), 0);
    // Clamp to prevent overflow
    if ((shift + 1) > 30)
    {
      x = fp64_make(0x40000000, 0);
    }
  }

  // Newton-Raphson iterations: x = x * (2 - b * x)
  two = fp64_make(2, 0);
  i = 0;
  while (i < 5)
  {
    bx = fp64_mul(b, x);
    correction = fp64_sub(two, bx);
    x = fp64_mul(x, correction);
    i = i + 1;
  }

  // Result = a * (1/b)
  x = fp64_mul(a, x);

  // Fix sign
  if (neg_a != neg_b)
  {
    x = fp64_neg(x);
  }

  return x;
}
