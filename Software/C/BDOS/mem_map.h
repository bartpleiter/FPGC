#ifndef MEM_MAP_H
#define MEM_MAP_H

//
// Memory map definitions for BDOS (byte-addressable).
//
// Byte Address   | Size      | Description
// ---------------+-----------+--------------------------------------------
// 0x0000000      |           | === KERNEL REGION ===
//                | 4 MiB     | Kernel Code + Data
// 0x03FFFFF      |           | Main and Interrupt Stacks at the end
// ---------------+-----------+--------------------------------------------
// 0x0400000      |           | === KERNEL HEAP ===
//                | 28 MiB    | Dynamic allocation
// 0x1FFFFFF      |           |
// ---------------+-----------+--------------------------------------------
// 0x2000000      |           | === USER PROGRAM REGION ===
//                |           | 8 x 2 MiB slots (16 MiB)
//                | 16 MiB    | Position-independent programs
//                |           | Programs can span multiple slots
// 0x2FFFFFF      |           | Program stack at the end of their last slot
// ---------------+-----------+--------------------------------------------
// 0x3000000      |           | === BRFS CACHE ===
//                | 16 MiB    | BRFS filesystem in RAM
// 0x3FFFFFF      |           |
// ---------------+-----------+--------------------------------------------
//

// ---- Physical Memory Limits (SDRAM) ----

#define MEM_PHYSICAL_START       0x0000000
#define MEM_PHYSICAL_END         0x4000000  // 64 MiB
#define MEM_PHYSICAL_SIZE        0x4000000

// ---- Kernel Region ----
// Note: make sure these addresses match in cgb32p3.inc in B32CC!

#define MEM_KERNEL_START         0x000000
#define MEM_KERNEL_SIZE          0x400000   // 4 MiB
#define MEM_KERNEL_END           0x400000

// Kernel stacks (at top of kernel region)
#define MEM_KERNEL_STACK_TOP     0x3DFFFC   // Main stack
#define MEM_KERNEL_SYSCALL_STACK_TOP 0x3EFFFC // Syscall stack
#define MEM_KERNEL_INT_STACK_TOP 0x3FFFFC   // Interrupt stack

// ---- Kernel Heap ----

#define MEM_HEAP_START           0x400000
#define MEM_HEAP_SIZE            0x1C00000  // 28 MiB
#define MEM_HEAP_END             0x2000000

// ---- User Program Region ----

#define MEM_PROGRAM_START        0x2000000
#define MEM_PROGRAM_END          0x3000000
#define MEM_PROGRAM_SIZE         0x1000000  // 16 MiB

// Slot configuration
#define MEM_SLOT_SIZE            0x200000   // 2 MiB per slot
#define MEM_SLOT_COUNT           8          // 16 MiB / 2 MiB = 8 slots

// ---- BRFS Cache (0x3000000 - 0x3FFFFFF) - 16 MiB ----

#define MEM_BRFS_START          0x3000000
#define MEM_BRFS_SIZE           0x1000000   // 16 MiB
#define MEM_BRFS_END            0x4000000

// ---- I/O Memory Map ----

#define MEM_IO_START            0x1C000000

// ---- CPU I/O Registers ----
#define MEM_IO_PC_BACKUP        0x1F000000  // Read/write interrupt return PC
#define MEM_IO_HW_STACK_PTR     0x1F000004  // Read/write hardware stack pointer

#endif // MEM_MAP_H
