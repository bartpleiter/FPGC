#ifndef USERLIB_DMA_H
#define USERLIB_DMA_H

/*
 * DMA driver for userBDOS programs.
 *
 * Wraps the FPGC DMAengine MMIO register block. Exposes the same
 * synchronous helpers as the libfpgc version (which BDOS itself uses),
 * but lives in userlib so userBDOS programs link without pulling in
 * libfpgc.
 *
 * All transfers must be 32-byte (cache-line) aligned in both endpoints
 * and in the byte count.
 */

/* DMA register block (MMIO). Mirrors libfpgc/include/fpgc.h. */
#define FPGC_DMA_SRC            0x1C000070
#define FPGC_DMA_DST            0x1C000074
#define FPGC_DMA_COUNT          0x1C000078
#define FPGC_DMA_CTRL           0x1C00007C
#define FPGC_DMA_STATUS         0x1C000080

/* DMA modes (low 4 bits of CTRL). */
#define FPGC_DMA_MODE_MEM2MEM   0
#define FPGC_DMA_MODE_MEM2SPI   1
#define FPGC_DMA_MODE_SPI2MEM   2
#define FPGC_DMA_MODE_MEM2VRAM  3

/* CTRL bit fields. */
#define FPGC_DMA_CTRL_IRQ_EN    (1u << 4)
#define FPGC_DMA_CTRL_SPI_SHIFT 5
#define FPGC_DMA_CTRL_START     (1u << 31)

/* STATUS bit fields. */
#define FPGC_DMA_STATUS_BUSY    (1u << 0)
#define FPGC_DMA_STATUS_DONE    (1u << 1)
#define FPGC_DMA_STATUS_ERROR   (1u << 2)

typedef enum {
    DMA_MEM2MEM  = FPGC_DMA_MODE_MEM2MEM,
    DMA_MEM2SPI  = FPGC_DMA_MODE_MEM2SPI,
    DMA_SPI2MEM  = FPGC_DMA_MODE_SPI2MEM,
    DMA_MEM2VRAM = FPGC_DMA_MODE_MEM2VRAM
} dma_mode_t;

/*
 * Synchronous SDRAM-to-SDRAM copy. Returns 0 on success, -1 on engine error
 * (alignment violation, count==0, etc.). Wraps the transfer with a
 * cache_flush_data() before and after.
 */
int dma_copy(unsigned int dst, unsigned int src, unsigned int count);

/*
 * Synchronous SDRAM-to-VRAMPX blit.
 *
 *   src must be 32-byte aligned in SDRAM, dst must be 32-byte aligned and
 *   lie in the VRAMPX byte range (0x1EC00000..0x1EC20000), count must be
 *   a multiple of 32. Returns 0 on success and -1 on engine error.
 *
 * Flushes the L1d cache before the transfer; no post-invalidate (VRAMPX
 * is write-only from the CPU side).
 */
int dma_blit_to_vram(unsigned int dst, unsigned int src, unsigned int count);

/*
 * Asynchronous start helpers; caller must poll dma_busy() / dma_status()
 * and is responsible for cache coherency.
 */
void dma_start_mem2mem(unsigned int dst, unsigned int src, unsigned int count);
void dma_start_mem2vram(unsigned int dst, unsigned int src, unsigned int count);

/* Returns non-zero while the engine is busy. */
int dma_busy(void);

/* Reads STATUS once (also clears the sticky done/error bits). */
unsigned int dma_status(void);

/*
 * Issue a `ccached` instruction (data-cache flush + invalidate).
 * Implemented in dma_asm.asm because cproc has no inline-asm support.
 */
void cache_flush_data(void);

#endif /* USERLIB_DMA_H */
