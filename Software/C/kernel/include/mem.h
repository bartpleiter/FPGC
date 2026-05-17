/*
 * mem.h — kernel memory layout and process memory allocator.
 *
 * v4 memory map (64 MiB SDRAM):
 *   0x000000 – 0x0FFFFF   Kernel code + BSS (1 MiB)
 *   0x100000 – 0x10FFFF   Kernel stacks (64 KiB: main, syscall, int)
 *   0x110000 – 0x1FFFFF   Kernel heap (~960 KiB)
 *   0x200000 – 0x1FFFFFF   Process memory pool (30 MiB)
 *   0x2000000 – 0x23FFFFF  BRFS SD cache (4 MiB)
 *   0x2400000 – 0x3FFFFFF  BRFS SPI cache (28 MiB)
 */
#ifndef KERNEL_MEM_H
#define KERNEL_MEM_H

/* Kernel stacks */
#define KERNEL_STACK_TOP       0x107FFC   /* Main stack (grows down within 64K) */
#define KERNEL_SYSCALL_STACK   0x10BFFC   /* Syscall stack */
#define KERNEL_INT_STACK       0x10FFFC   /* Interrupt stack */

/* Kernel heap */
#define KERNEL_HEAP_START      0x110000
#define KERNEL_HEAP_END        0x200000
#define KERNEL_HEAP_SIZE       (KERNEL_HEAP_END - KERNEL_HEAP_START)

/* Process memory pool */
#define PROC_POOL_START        0x200000
#define PROC_POOL_END          0x2000000
#define PROC_POOL_SIZE         (PROC_POOL_END - PROC_POOL_START)

/* Minimum and default process allocation */
#define PROC_MEM_MIN           0x10000     /* 64 KiB minimum */
#define PROC_STACK_SIZE        0x40000     /* 256 KiB stack per process */
#define PROC_GROW_CHUNK        0x100000    /* 1 MiB growth granularity */
#define PROC_MEM_ALIGN         0x20        /* 32-byte alignment (cache line) */

/* BRFS cache regions */
#define BRFS_SD_CACHE_START    0x2000000
#define BRFS_SD_CACHE_END      0x2400000
#define BRFS_SPI_CACHE_START   0x2400000
#define BRFS_SPI_CACHE_END     0x4000000

/*
 * Process memory allocator — first-fit free list.
 *
 * Each process gets a contiguous region from the pool.
 * Freed regions are coalesced with neighbors.
 */

/* Initialize the process memory allocator */
void mem_init(void);

/* Allocate a contiguous region of at least `size` bytes.
 * Returns the base address, or 0 on failure. */
unsigned int mem_alloc(unsigned int size);

/* Free a previously allocated region (by base + size). */
void mem_free_region(unsigned int base, unsigned int size);

/* Return total free bytes in the process pool. */
unsigned int mem_free_total(void);

/* Try to grow an existing allocation in-place.
 * Returns bytes absorbed (>= growth), or 0 on failure. */
unsigned int mem_grow_region(unsigned int base, unsigned int old_size,
                             unsigned int growth);

/*
 * Kernel heap — simple bump allocator.
 * Used for process table, file tables, buffers, etc.
 */
void          kheap_init(void);
void         *kheap_alloc(unsigned int bytes);
unsigned int  kheap_mark(void);
void          kheap_release(unsigned int mark);

#endif /* KERNEL_MEM_H */
