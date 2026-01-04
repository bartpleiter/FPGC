#ifndef MEM_MAP_H
#define MEM_MAP_H

/*
 * Memory map definitions for BDOS.
 *
 * Word Address   | Size      | Description
 * ---------------+-----------+--------------------------------------------
 * 0x0000000      |           | === KERNEL REGION ===
 *                | 512 KiW   | Kernel Code + Data (2 MiB)
 * 0x0080000      |           | Main and Interrupt Stacks at the end
 * ---------------+-----------+--------------------------------------------
 * 0x0080000      |           | === KERNEL HEAP ===
 *                | 512 KiW   | Dynamic allocation (2 MiB)
 * 0x0100000      |           |
 * ---------------+-----------+--------------------------------------------
 * 0x0100000      |           | === USER PROGRAM REGION ===
 *                |           | 14 × 512 KiW slots (28 MiB)
 *                | 7 MiW     | Position-independent programs
 *                |           | Programs can span multiple slots
 * 0x0800000      |           | Program stack at the end of their last slot
 * ---------------+-----------+--------------------------------------------
 * 0x0800000      |           | === BRFS CACHE ===
 *                | 8 MiW     | BRFS filesystem in RAM (32 MiB)
 * 0x1000000      |           | (End of 64 MiB physical memory)
 * ---------------+-----------+--------------------------------------------
*/

// =============================================================================
// PHYSICAL MEMORY LIMITS (SDRAM)
// =============================================================================
#define MEM_PHYSICAL_START       0x0000000
#define MEM_PHYSICAL_END         0x1000000   // 16 MiW = 64 MiB
#define MEM_PHYSICAL_SIZE        0x1000000

// =============================================================================
// KERNEL REGION
// Note: make sure these addresses match in cgb32p3.inc in B32CC!
// =============================================================================
#define MEM_KERNEL_START         0x0000000
#define MEM_KERNEL_SIZE          0x0080000   // 512 KiW (2 MiB)
#define MEM_KERNEL_END           0x0080000

// Kernel stacks (at top of kernel region)
#define MEM_KERNEL_STACK_TOP     0x007DFFF  // Main stack
#define MEM_KERNEL_INT_STACK_TOP 0x007FFFF  // Interrupt stack

// =============================================================================
// KERNEL HEAP
// =============================================================================
#define MEM_HEAP_START          0x0080000
#define MEM_HEAP_SIZE           0x0080000   // 512 KiW (2 MiB)
#define MEM_HEAP_END            0x0100000

// =============================================================================
// USER PROGRAM REGION
// =============================================================================
#define MEM_PROGRAM_START       0x0100000
#define MEM_PROGRAM_END         0x0800000
#define MEM_PROGRAM_SIZE        0x0700000   // 7 MiW (28 MiB)

// Slot configuration
#define MEM_SLOT_SIZE           0x0080000   // 512 KiW (2 MiB) per slot
#define MEM_SLOT_COUNT          14          // 7 MiW / 512 KiW = 14 slots

// =============================================================================
// BRFS CACHE (0x0800000 - 0x0FFFFFF) - 32 MiB
// =============================================================================
#define MEM_BRFS_START          0x0800000
#define MEM_BRFS_SIZE           0x0800000   // 8 MiW (32 MiB)
#define MEM_BRFS_END            0x1000000

// =============================================================================
// I/O MEMORY MAP
// =============================================================================
#define MEM_IO_START            0x7000000

#endif // MEM_MAP_H
