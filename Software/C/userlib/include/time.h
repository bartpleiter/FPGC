#ifndef TIME_H
#define TIME_H

/*
 * time.h — Hardware microsecond counter for user programs.
 *
 * Reads the microsecond counter via hwio_read().
 * Requires hwio.asm to be linked into the binary.
 */

/* Return the current value of the hardware microsecond counter. */
unsigned int get_micros(void);

#endif /* TIME_H */
