#ifndef FIXED64_H
#define FIXED64_H

// fixed64 — Q32.32 signed fixed-point arithmetic library.
//
// Uses the FP64 hardware coprocessor when available (via compiler intrinsics).
// Each value is represented as two 32-bit words: hi (signed integer part)
// and lo (unsigned fractional part).
//
// Value = hi + lo / 2^32
//
// Example: 3.75 → hi=3, lo=0xC0000000
//          -1.5 → hi=-2, lo=0x80000000  (two's complement)

// A Q32.32 fixed-point value stored as two 32-bit words.
struct fp64
{
  int hi;            // Signed integer part
  unsigned int lo;   // Unsigned fractional part
};

// ---- Construction ----

// Create an fp64 from integer and fractional parts.
struct fp64 fp64_make(int hi, unsigned int lo);

// Create an fp64 from an integer (fractional part = 0).
struct fp64 fp64_from_int(int val);

// Create an fp64 from a 16.16 fixed-point value.
struct fp64 fp64_from_fp16(int fp16_val);

// ---- Arithmetic (uses FP64 coprocessor) ----

// a + b
struct fp64 fp64_add(struct fp64 a, struct fp64 b);

// a - b
struct fp64 fp64_sub(struct fp64 a, struct fp64 b);

// a * b
struct fp64 fp64_mul(struct fp64 a, struct fp64 b);

// Negate: -a
struct fp64 fp64_neg(struct fp64 a);

// Absolute value
struct fp64 fp64_abs(struct fp64 a);

// ---- Comparison ----

// Returns negative if a < b, 0 if a == b, positive if a > b.
int fp64_cmp(struct fp64 a, struct fp64 b);

// ---- Conversion ----

// Extract the integer part (truncates toward zero).
int fp64_to_int(struct fp64 val);

// Convert to 16.16 fixed-point (truncates low 16 fractional bits).
int fp64_to_fp16(struct fp64 val);

// ---- Shift operations ----

// Shift right by n bits (arithmetic, preserves sign). n must be 0..63.
struct fp64 fp64_shr(struct fp64 val, int n);

// Shift left by n bits. n must be 0..63.
struct fp64 fp64_shl(struct fp64 val, int n);

// ---- Division (software, using Newton-Raphson) ----

// a / b  (approximately ~300 cycles)
struct fp64 fp64_div(struct fp64 a, struct fp64 b);

// ---- Constants ----

// Common constants
#define FP64_ZERO       fp64_make(0, 0)
#define FP64_ONE        fp64_make(1, 0)
#define FP64_MINUS_ONE  fp64_make(-1, 0)
#define FP64_HALF       fp64_make(0, 0x80000000u)

#endif // FIXED64_H
