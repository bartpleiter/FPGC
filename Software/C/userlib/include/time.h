#ifndef TIME_H
#define TIME_H

/*
 * time.h — Hardware microsecond counter for user programs.
 *
 * Reads the microsecond counter via __builtin_load().
 */

/* Hardware microsecond counter (read-only) */
#define MICROS_ADDR      0x1C000068

/* Return the current value of the hardware microsecond counter. */
unsigned int get_micros(void);

#endif /* TIME_H */
