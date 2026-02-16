#ifndef MEM_MAP_H
#define MEM_MAP_H

/*
 * Memory map definitions for BDOS.
 *
 * Word Address   | Size      | Description
 * ---------------+-----------+--------------------------------------------
 * 0x000000       |           | === KERNEL REGION ===
 *                | 1 MiW     | Kernel Code + Data (4 MiB)
 * 0x0FFFFF       |           | Main and Interrupt Stacks at the end
 * ---------------+-----------+--------------------------------------------
 * 0x100000       |           | === KERNEL HEAP ===
 *                | 7 MiW     | Dynamic allocation (28 MiB)
 * 0x7FFFFF       |           |
 * ---------------+-----------+--------------------------------------------
 * 0x800000       |           | === USER PROGRAM REGION ===
 *                |           | 8 × 512 KiW slots (16 MiB)
 *                | 4 MiW     | Position-independent programs
 *                |           | Programs can span multiple slots
 * 0xBFFFFF       |           | Program stack at the end of their last slot
 * ---------------+-----------+--------------------------------------------
 * 0xC00000       |           | === BRFS CACHE ===
 *                | 4 MiW     | BRFS filesystem in RAM (16 MiB)
 * 0xFFFFFF       |           | 
 * ---------------+-----------+--------------------------------------------
*/

// =============================================================================
// PHYSICAL MEMORY LIMITS (SDRAM)
// =============================================================================
#define MEM_PHYSICAL_START       0x0000000
#define MEM_PHYSICAL_END         0x1000000  // 16 MiW = 64 MiB
#define MEM_PHYSICAL_SIZE        0x1000000

// =============================================================================
// KERNEL REGION
// Note: make sure these addresses match in cgb32p3.inc in B32CC!
// =============================================================================
#define MEM_KERNEL_START         0x000000
#define MEM_KERNEL_SIZE          0x100000   // 1 MiW (4 MiB)
#define MEM_KERNEL_END           0x100000

// Kernel stacks (at top of kernel region)
#define MEM_KERNEL_STACK_TOP     0x0FBFFF   // Main stack
#define MEM_KERNEL_INT_STACK_TOP 0x0FFFFF   // Interrupt stack

// =============================================================================
// KERNEL HEAP
// =============================================================================
#define MEM_HEAP_START           0x100000
#define MEM_HEAP_SIZE            0x700000   // 7 MiW (28 MiB)
#define MEM_HEAP_END             0x800000

// =============================================================================
// USER PROGRAM REGION
// =============================================================================
#define MEM_PROGRAM_START        0x800000
#define MEM_PROGRAM_END          0xC00000
#define MEM_PROGRAM_SIZE         0x400000   // 4 MiW (16 MiB)

// Slot configuration
#define MEM_SLOT_SIZE            0x080000   // 512 KiW (2 MiB) per slot
#define MEM_SLOT_COUNT           8          // 4 MiW / 512 KiW = 8 slots

// =============================================================================
// BRFS CACHE (0xC00000 - 0xFFFFFF) - 16 MiB
// =============================================================================
#define MEM_BRFS_START          0xC00000
#define MEM_BRFS_SIZE           0x400000    // 4 MiW (16 MiB)
#define MEM_BRFS_END            0x1000000

// =============================================================================
// I/O MEMORY MAP
// =============================================================================
#define MEM_IO_START            0x7000000

#endif // MEM_MAP_H
