#ifndef FPGC_DMA_H
#define FPGC_DMA_H

/*
 * DMA driver for the FPGC DMAengine.
 *
 * Currently supports memory-to-memory copies and SPI<->memory bursts on
 * SPI0 (Flash 1) and SPI4 (Ethernet). The other modes (MEM2VRAM, MEM2IO,
 * IO2MEM) will be added later.
 *
 * All transfers must be 32-byte (cache-line) aligned in both the
 * memory addresses they touch and the byte count.
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
    DMA_IO2MEM   = FPGC_DMA_MODE_IO2MEM,
    DMA_SPI2MEM_QSPI = FPGC_DMA_MODE_SPI2MEM_QSPI,
    DMA_CAM2MEM  = FPGC_DMA_MODE_CAM2MEM
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

/*
 * Synchronous SDRAM-to-VRAMPX blit.
 *
 *   src must be 32-byte aligned in SDRAM, dst must be 32-byte aligned and
 *   lie in the VRAMPX byte range (0x1EC00000 .. 0x1EC20000), and count
 *   must be a multiple of 32. Returns 0 on success and -1 on engine error.
 *
 * Flushes the L1 data cache before the transfer so the engine reads the
 * latest source bytes; VRAMPX is write-only from the CPU side so no
 * post-invalidate is needed.
 */
int dma_blit_to_vram(unsigned int dst, unsigned int src, unsigned int count);

/*
 * Asynchronous start of a MEM2VRAM transfer. Caller must poll dma_busy()
 * and is responsible for cache coherency.
 */
void dma_start_mem2vram(unsigned int dst, unsigned int src, unsigned int count);

/*
 * Asynchronous SPI<->memory burst on the given SPI controller (0 or 4).
 * The mode argument selects direction: DMA_SPI2MEM or DMA_MEM2SPI.
 *
 *   - For SPI2MEM, `dst` is the SDRAM destination (32-byte aligned) and
 *     `src` is ignored. `count` bytes are read from the SPI peripheral
 *     into SDRAM. The caller is responsible for issuing the read command
 *     and 24-bit address before calling, and for holding CS low across
 *     the call.
 *   - For MEM2SPI, `src` is the SDRAM source (32-byte aligned) and `dst`
 *     is ignored. `count` bytes are pushed out to the SPI peripheral.
 *     The caller is responsible for issuing the page-program command
 *     and address before calling, and for holding CS low across the call.
 *
 * No cache flushing is performed; callers should ccached as needed.
 */
void dma_start_spi(dma_mode_t mode, int spi_id, unsigned int dst,
                   unsigned int src, unsigned int count);

/*
 * Asynchronous SPI2MEM QSPI Fast Read on SPI1 (BRFS flash).
 *
 * The DMA engine drives QSPIflash to issue opcode 0xEB + 24-bit
 * `qspi_addr` + M=0xA5 + 4 dummy SCK + read of `count` bytes (4 bits
 * per SCK). The caller is responsible for asserting CS low before this
 * call and CS high after `dma_busy()` returns 0. `dst` must be 32-byte
 * aligned in SDRAM and `count` must be a multiple of 32.
 *
 * No cache flushing is performed; callers should ccached as needed.
 */
void dma_start_spi_qspi_read(int spi_id, unsigned int dst,
                             unsigned int qspi_addr, unsigned int count);

/*
 * Asynchronous camera-to-memory DMA transfer.
 *
 * The DMA engine waits for CameraCapture to produce 256-bit cache lines
 * and writes them to SDRAM at `dst`, advancing by 32 bytes each time.
 * `count` is the total byte count (must be 32-byte aligned, >0).
 * For a QVGA frame: dst = buffer address, count = 76800 (320×240 Y bytes).
 *
 * No cache flushing is performed; the camera writes bypass the CPU cache.
 */
void dma_start_cam(unsigned int dst, unsigned int count);
void dma_start_cam_immediate(unsigned int dst, unsigned int count);

/* Returns non-zero while the engine is busy. */
int dma_busy(void);

/*
 * Read DMA_STATUS once. Reading clears the sticky `done` and `error`
 * bits, so callers should typically only read it once per transfer.
 */
unsigned int dma_status(void);

/*
 * Load one entry into the DMA's 256×8 auto-contrast LUT.
 * addr = input pixel value (0-255), data = output pixel value.
 * Call 256 times to fill the entire table.
 */
void dma_lut_write(int addr, int data);

/*
 * Load a 4-shade dither threshold table entry.
 * table = 0 (thresh_0), 1 (thresh_1), 2 (thresh_2).
 * mi = matrix index (0-15), value = threshold byte.
 */
void dma_dither_thresh_write(int table, int mi, int value);

/*
 * Load an 8-shade Bayer offset table entry.
 * mi = matrix index (0-15), value = offset byte.
 */
void dma_dither_bayer_write(int mi, int value);

/*
 * Asynchronous MEM2VRAM with optional LUT and/or dithering.
 * flags: OR of FPGC_DMA_CTRL_LUT_EN, FPGC_DMA_CTRL_DITHER_EN,
 *        FPGC_DMA_CTRL_DITHER_8.
 */
void dma_start_mem2vram_ex(unsigned int dst, unsigned int src,
                           unsigned int count, unsigned int flags);

/*
 * Flush + invalidate the L1 data cache (ccached instruction). Used by
 * dma_copy() but exposed for callers that drive the engine directly.
 */
void cache_flush_data(void);

#endif /* FPGC_DMA_H */
