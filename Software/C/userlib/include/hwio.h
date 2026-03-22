#ifndef HWIO_H
#define HWIO_H

/*
 * hwio.h — Memory-mapped I/O access for user programs.
 *
 * The actual implementation is in libc/sys/hwio.asm (linked into the binary).
 * cproc does not support volatile, so all MMIO goes through these asm helpers.
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
