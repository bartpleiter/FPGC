#ifndef TIME_H
#define TIME_H

// Hardware microsecond counter address (read-only, memory-mapped I/O register)
#define MICROS_ADDR 0x1C000068

// Return the current value of the hardware microsecond counter.
unsigned int get_micros();

#endif // TIME_H
