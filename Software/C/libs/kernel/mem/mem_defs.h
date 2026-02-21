#ifndef MEM_DEFS_H
#define MEM_DEFS_H

// Memory Definitions
// This header defines memory layout constants for the FPGC system.
// The goal is to have a single place with an overview of what memory regions are reserved for what purpose.

#define MEM_KERNEL_STACK_START      0x77FFFF // Kernel stack (grows downwards)
#define MEM_KERNEL_INT_STACK_START  0x7FFFFF // Kernel interrupt stack (grows downwards)
#define MEM_BRFS_CACHE_START        0x800000 // BRFS RAM cache (size = 8MiW)

#endif // MEM_DEFS_H
