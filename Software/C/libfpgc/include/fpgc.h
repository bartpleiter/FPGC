#ifndef FPGC_H
#define FPGC_H

/*========================================================================
 * Memory-mapped I/O builtins
 *
 * cproc does not support volatile stores, so all memory-mapped I/O
 * uses compiler builtins that emit inline write/read instructions:
 *   __builtin_store(addr, value)   — inline word store
 *   __builtin_storeb(addr, value)  — inline byte store
 *   __builtin_load(addr)           — inline word load
 *   __builtin_loadb(addr)          — inline byte load
 *======================================================================*/

/*========================================================================
 * Memory-Mapped I/O Addresses
 *======================================================================*/

/* UART */
#define FPGC_UART_TX        0x1C000000
#define FPGC_UART_RX        0x1C000004

/* Timers (3 hardware timers: value + trigger registers) */
#define FPGC_TIMER0_VAL     0x1C000008
#define FPGC_TIMER0_CTRL    0x1C00000C
#define FPGC_TIMER1_VAL     0x1C000010
#define FPGC_TIMER1_CTRL    0x1C000014
#define FPGC_TIMER2_VAL     0x1C000018
#define FPGC_TIMER2_CTRL    0x1C00001C

/* SPI buses (6 buses, each with data + chip select registers) */
#define FPGC_SPI0_DATA      0x1C000020
#define FPGC_SPI0_CS        0x1C000024
#define FPGC_SPI1_DATA      0x1C000028
#define FPGC_SPI1_CS        0x1C00002C
#define FPGC_SPI2_DATA      0x1C000030
#define FPGC_SPI2_CS        0x1C000034
#define FPGC_SPI3_DATA      0x1C00003C
#define FPGC_SPI3_CS        0x1C000040
#define FPGC_SPI4_DATA      0x1C000048
#define FPGC_SPI4_CS        0x1C00004C
#define FPGC_SPI5_DATA      0x1C000054
#define FPGC_SPI5_CS        0x1C000058

/* CH376 USB host interrupt pins (active low) */
#define FPGC_CH376_TOP_NINT   0x1C000038
#define FPGC_CH376_BOT_NINT   0x1C000044

/* System registers */
#define FPGC_BOOT_MODE      0x1C000064
#define FPGC_MICROS          0x1C000068
#define FPGC_USER_LED       0x1C00006C

/* DMA engine registers (see Docs/plans/dma-implementation-plan.md §2.9) */
#define FPGC_DMA_SRC        0x1C000070
#define FPGC_DMA_DST        0x1C000074
#define FPGC_DMA_COUNT      0x1C000078
#define FPGC_DMA_CTRL       0x1C00007C
#define FPGC_DMA_STATUS     0x1C000080
#define FPGC_DMA_QSPI_ADDR  0x1C000084

/* Camera subsystem registers */
#define FPGC_CAM_CTRL       0x1C000088
#define FPGC_CAM_STATUS     0x1C00008C
#define FPGC_CAM_SCCB       0x1C000090
#define FPGC_CAM_BUF0       0x1C000094
#define FPGC_CAM_BUF1       0x1C000098

/* DMA_CTRL bit fields */
#define FPGC_DMA_MODE_MEM2MEM   0
#define FPGC_DMA_MODE_MEM2SPI   1
#define FPGC_DMA_MODE_SPI2MEM   2
#define FPGC_DMA_MODE_MEM2VRAM  3
#define FPGC_DMA_MODE_MEM2IO    4
#define FPGC_DMA_MODE_IO2MEM    5
#define FPGC_DMA_MODE_SPI2MEM_QSPI 6
#define FPGC_DMA_CTRL_IRQ_EN    (1u << 4)
#define FPGC_DMA_CTRL_SPI_SHIFT 5
#define FPGC_DMA_CTRL_START     (1u << 31)

/* DMA_STATUS bits (sticky bits cleared on read) */
#define FPGC_DMA_STATUS_BUSY    (1u << 0)
#define FPGC_DMA_STATUS_DONE    (1u << 1)
#define FPGC_DMA_STATUS_ERROR   (1u << 2)

/* Interrupt system */
#define FPGC_PC_BACKUP      0x1F000000
#define FPGC_HW_STACK_PTR   0x1F000004

/* GPU VRAM regions */
#define FPGC_GPU_PATTERN_TABLE   0x1E400000  /* 4096 bytes: 256 chars × 16 bytes */
#define FPGC_GPU_PALETTE_TABLE   0x1E401000  /* 128 bytes: 32 palettes × 4 bytes */

#define FPGC_GPU_BG_TILE_TABLE   0x1E800000
#define FPGC_GPU_BG_COLOR_TABLE  0x1E802000
#define FPGC_GPU_PARAMS          0x1E808000  /* scroll X, scroll Y */

#define FPGC_GPU_WIN_TILE_TABLE  0x1E804000
#define FPGC_GPU_WIN_COLOR_TABLE 0x1E806000

#define FPGC_GPU_PIXEL_DATA      0x1EC00000  /* 320×240 bytes */
#define FPGC_GPU_PIXEL_PALETTE   0x1EC80000  /* 256 entries × 3 bytes (RGB) */

/*========================================================================
 * Memory Layout Constants
 *======================================================================*/

/* Physical memory (64 MiB SDRAM) */
#define FPGC_MEM_START           0x0000000
#define FPGC_MEM_END             0x4000000

/* Kernel region */
#define FPGC_KERNEL_START        0x000000
#define FPGC_KERNEL_END          0x400000
#define FPGC_KERNEL_STACK_TOP    0x3DFFFC
#define FPGC_SYSCALL_STACK_TOP   0x3EFFFC
#define FPGC_INT_STACK_TOP       0x3FFFFC

/* Kernel heap */
#define FPGC_HEAP_START          0x400000
#define FPGC_HEAP_END            0x2000000

/* User program region */
#define FPGC_PROGRAM_START       0x2000000
#define FPGC_PROGRAM_END         0x2C00000
#define FPGC_SLOT_SIZE           0x200000
#define FPGC_SLOT_COUNT          6

/* BRFS cache */
#define FPGC_BRFS_SD_START       0x2C00000
#define FPGC_BRFS_SD_END         0x3000000
#define FPGC_BRFS_START          0x3000000
#define FPGC_BRFS_END            0x4000000

/*========================================================================
 * Interrupt IDs
 *======================================================================*/

#define FPGC_INTID_UART          1
#define FPGC_INTID_TIMER0        2
#define FPGC_INTID_TIMER1        3
#define FPGC_INTID_TIMER2        4
#define FPGC_INTID_FRAME_DRAWN   5
#define FPGC_INTID_ETH           6
#define FPGC_INTID_DMA           7

/*========================================================================
 * SPI bus identifiers
 *======================================================================*/

#define FPGC_SPI_FLASH_0    0
#define FPGC_SPI_FLASH_1    1
#define FPGC_SPI_USB_0      2
#define FPGC_SPI_USB_1      3
#define FPGC_SPI_ETH        4
#define FPGC_SPI_SD_CARD    5

/*========================================================================
 * GPU constants
 *======================================================================*/

#define FPGC_GPU_SCREEN_W   320
#define FPGC_GPU_SCREEN_H   240
#define FPGC_GPU_TILE_W     40   /* 320 / 8 */
#define FPGC_GPU_TILE_H     30   /* 240 / 8 */

#endif /* FPGC_H */
