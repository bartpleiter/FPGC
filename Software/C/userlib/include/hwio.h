#ifndef HWIO_H
#define HWIO_H

/*
 * hwio.h — Memory-mapped I/O access for user programs.
 *
 * For performance-critical code (e.g., pixel rendering loops), prefer the
 * compiler builtins which emit inline write/read instructions with no
 * function call overhead:
 *
 *   __builtin_store(addr, value)   — word store  (write instruction)
 *   __builtin_storeb(addr, value)  — byte store  (writeb instruction)
 *   __builtin_load(addr)           — word load   (read instruction)
 *   __builtin_loadb(addr)          — byte load   (readb instruction)
 *
 * The hwio_write/hwio_read functions below are assembly helpers that work
 * correctly but incur ~10 cycles of call/return overhead per invocation.
 * Use them for non-performance-critical I/O or when compatibility with
 * code that can't use builtins is needed.
 */

extern void hwio_write(int addr, int value);
extern int  hwio_read(int addr);

/* GPU pixel framebuffer */
#define PIXEL_FB_ADDR    0x1EC00000
#define PIXEL_FB_WIDTH   320
#define PIXEL_FB_HEIGHT  240

/* GPU pixel palette */
#define PIXEL_PALETTE_ADDR 0x1EC80000

/* Hardware microsecond counter (read-only) */
#define MICROS_ADDR      0x1C000068

#endif /* HWIO_H */
