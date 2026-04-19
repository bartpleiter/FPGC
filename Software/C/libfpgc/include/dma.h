#ifndef FPGC_DMA_H
#define FPGC_DMA_H

/*
 * DMA driver for the FPGC DMAengine.
 *
 * Phase 1 (this commit) only supports memory-to-memory copies; the other
 * modes (MEM2SPI, SPI2MEM, MEM2VRAM, MEM2IO, IO2MEM) will be added later.
 *
 * All MEM2MEM transfers must be 32-byte (cache-line) aligned in both the
 * source/destination addresses and the byte count.
 *
 * Coherency: dma_copy() flushes/invalidates the L1 data cache around the
 * transfer with the `ccached` instruction, so callers do not need to do
 * that themselves.
 */

#include "fpgc.h"

/* DMA transfer modes (mirrors FPGC_DMA_MODE_* in fpgc.h). */
typedef enum {
    DMA_MEM2MEM  = FPGC_DMA_MODE_MEM2MEM,
    DMA_MEM2SPI  = FPGC_DMA_MODE_MEM2SPI,
    DMA_SPI2MEM  = FPGC_DMA_MODE_SPI2MEM,
    DMA_MEM2VRAM = FPGC_DMA_MODE_MEM2VRAM,
    DMA_MEM2IO   = FPGC_DMA_MODE_MEM2IO,
    DMA_IO2MEM   = FPGC_DMA_MODE_IO2MEM
} dma_mode_t;

/*
 * Synchronous memory-to-memory DMA copy.
 *
 *   src, dst, count must each be a multiple of 32. Returns 0 on success
 *   and -1 on engine error (alignment violation, unsupported mode, etc.).
 *
 * Wraps the transfer with cache_flush_data() before and after so the L1d
 * sees consistent data on either side.
 */
int dma_copy(unsigned int dst, unsigned int src, unsigned int count);

/*
 * Asynchronous start; caller must poll dma_busy() / dma_status().
 * No cache flushing is performed -- the caller is responsible for keeping
 * SDRAM coherent with the L1 data cache.
 */
void dma_start_mem2mem(unsigned int dst, unsigned int src, unsigned int count);

/* Returns non-zero while the engine is busy. */
int dma_busy(void);

/*
 * Read DMA_STATUS once. Reading clears the sticky `done` and `error`
 * bits, so callers should typically only read it once per transfer.
 */
unsigned int dma_status(void);

/*
 * Flush + invalidate the L1 data cache (ccached instruction). Used by
 * dma_copy() but exposed for callers that drive the engine directly.
 */
void cache_flush_data(void);

#endif /* FPGC_DMA_H */
