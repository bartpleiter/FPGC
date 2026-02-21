#ifndef DEBUG_H
#define DEBUG_H

// Debugging Utilities for Memory Management

// Dump a memory region in hexadecimal format over UART
void debug_mem_dump(unsigned int *start, unsigned int length);

#endif // DEBUG_H
