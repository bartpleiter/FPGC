#ifndef BDOS_MEM_MAP_H
#define BDOS_MEM_MAP_H

#include "fpgc.h"

/*========================================================================
 * Aliases for BDOS-specific naming (all reference fpgc.h constants)
 *======================================================================*/

/* Kernel stacks */
#define MEM_KERNEL_STACK_TOP      FPGC_KERNEL_STACK_TOP
#define MEM_SYSCALL_STACK_TOP     FPGC_SYSCALL_STACK_TOP
#define MEM_INT_STACK_TOP         FPGC_INT_STACK_TOP

/* Kernel heap */
#define MEM_HEAP_START            FPGC_HEAP_START
#define MEM_HEAP_END              FPGC_HEAP_END

/* User program region */
#define MEM_PROGRAM_START         FPGC_PROGRAM_START
#define MEM_PROGRAM_END           FPGC_PROGRAM_END
#define MEM_SLOT_SIZE             FPGC_SLOT_SIZE
#define MEM_SLOT_COUNT            FPGC_SLOT_COUNT

/* SD card BRFS cache */
#define MEM_SD_CACHE_START        FPGC_BRFS_SD_START
#define MEM_SD_CACHE_END          FPGC_BRFS_SD_END
#define MEM_SD_CACHE_SIZE         (FPGC_BRFS_SD_END - FPGC_BRFS_SD_START)

/* BRFS cache */
#define MEM_BRFS_START            FPGC_BRFS_START
#define MEM_BRFS_END              FPGC_BRFS_END
#define MEM_BRFS_SIZE             (FPGC_BRFS_END - FPGC_BRFS_START)

/* I/O registers */
#define MEM_IO_PC_BACKUP          FPGC_PC_BACKUP
#define MEM_IO_HW_STACK_PTR       FPGC_HW_STACK_PTR

/* Slot helpers */
#define SLOT_ENTRY(n)       (MEM_PROGRAM_START + (n) * MEM_SLOT_SIZE)
#define SLOT_STACK_TOP(n)   (MEM_PROGRAM_START + ((n) + 1) * MEM_SLOT_SIZE - 4)

/* Program slot status codes */
#define SLOT_STATUS_EMPTY      0
#define SLOT_STATUS_RUNNING    1
#define SLOT_STATUS_SUSPENDED  2

/* No active slot */
#define SLOT_NONE (-1)

/* Aliases used throughout BDOS code */
#define BDOS_SLOT_NONE              SLOT_NONE
#define BDOS_SLOT_STATUS_EMPTY      SLOT_STATUS_EMPTY
#define BDOS_SLOT_STATUS_RUNNING    SLOT_STATUS_RUNNING
#define BDOS_SLOT_STATUS_SUSPENDED  SLOT_STATUS_SUSPENDED

/* Interrupt ID aliases (map to FPGC_INTID_* from fpgc.h) */
#define INTID_UART          FPGC_INTID_UART
#define INTID_TIMER0        FPGC_INTID_TIMER0
#define INTID_TIMER1        FPGC_INTID_TIMER1
#define INTID_TIMER2        FPGC_INTID_TIMER2
#define INTID_FRAME_DRAWN   FPGC_INTID_FRAME_DRAWN
#define INTID_ETH           FPGC_INTID_ETH

#endif /* BDOS_MEM_MAP_H */
