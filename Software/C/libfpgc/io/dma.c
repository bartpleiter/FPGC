#include "dma.h"
#include "fpgc.h"

int
dma_busy(void)
{
    /* Read STATUS without clearing semantics on the busy bit (busy is live). */
    return (int)(__builtin_load(FPGC_DMA_STATUS) & FPGC_DMA_STATUS_BUSY);
}

unsigned int
dma_status(void)
{
    return (unsigned int)__builtin_load(FPGC_DMA_STATUS);
}

void
dma_start_mem2mem(unsigned int dst, unsigned int src, unsigned int count)
{
    __builtin_store(FPGC_DMA_SRC,   (int)src);
    __builtin_store(FPGC_DMA_DST,   (int)dst);
    __builtin_store(FPGC_DMA_COUNT, (int)count);
    __builtin_store(FPGC_DMA_CTRL,
        (int)(FPGC_DMA_CTRL_START | (unsigned int)FPGC_DMA_MODE_MEM2MEM));
}

void
dma_start_mem2vram(unsigned int dst, unsigned int src, unsigned int count)
{
    __builtin_store(FPGC_DMA_SRC,   (int)src);
    __builtin_store(FPGC_DMA_DST,   (int)dst);
    __builtin_store(FPGC_DMA_COUNT, (int)count);
    __builtin_store(FPGC_DMA_CTRL,
        (int)(FPGC_DMA_CTRL_START | (unsigned int)FPGC_DMA_MODE_MEM2VRAM));
}

void
dma_start_spi(dma_mode_t mode, int spi_id, unsigned int dst,
              unsigned int src, unsigned int count)
{
    unsigned int ctrl;
    ctrl = FPGC_DMA_CTRL_START
         | ((unsigned int)spi_id << FPGC_DMA_CTRL_SPI_SHIFT)
         | (unsigned int)mode;
    __builtin_store(FPGC_DMA_SRC,   (int)src);
    __builtin_store(FPGC_DMA_DST,   (int)dst);
    __builtin_store(FPGC_DMA_COUNT, (int)count);
    __builtin_store(FPGC_DMA_CTRL,  (int)ctrl);
}

void
dma_start_spi_qspi_read(int spi_id, unsigned int dst,
                        unsigned int qspi_addr, unsigned int count)
{
    unsigned int ctrl;
    ctrl = FPGC_DMA_CTRL_START
         | ((unsigned int)spi_id << FPGC_DMA_CTRL_SPI_SHIFT)
         | (unsigned int)FPGC_DMA_MODE_SPI2MEM_QSPI;
    __builtin_store(FPGC_DMA_DST,       (int)dst);
    __builtin_store(FPGC_DMA_COUNT,     (int)count);
    __builtin_store(FPGC_DMA_QSPI_ADDR, (int)(qspi_addr & 0xFFFFFFu));
    __builtin_store(FPGC_DMA_CTRL,      (int)ctrl);
}

int
dma_copy(unsigned int dst, unsigned int src, unsigned int count)
{
    unsigned int status;

    /* Push any dirty L1d lines back to SDRAM so the engine reads fresh data. */
    cache_flush_data();

    dma_start_mem2mem(dst, src, count);

    /*
     * Spin-read STATUS until the engine is no longer busy.
     *
     * We must NOT call dma_busy() followed by dma_status() separately:
     * the Verilog status_read_pulse clears the sticky done/error bits on
     * each read, so a dma_busy() read that sees busy=0 would clear the
     * error flag before dma_status() can observe it. Instead, read STATUS
     * in the loop and exit with the value that first shows busy=0.
     */
    do {
        status = dma_status();
    } while (status & FPGC_DMA_STATUS_BUSY);

    /* Invalidate L1d so subsequent CPU reads see the new SDRAM contents. */
    cache_flush_data();

    if (status & FPGC_DMA_STATUS_ERROR) {
        return -1;
    }
    return 0;
}

int
dma_blit_to_vram(unsigned int dst, unsigned int src, unsigned int count)
{
    unsigned int status;

    /* Push any dirty L1d lines back to SDRAM so the engine reads fresh data. */
    cache_flush_data();

    dma_start_mem2vram(dst, src, count);

    /* Same single-read loop as dma_copy to avoid losing the error flag. */
    do {
        status = dma_status();
    } while (status & FPGC_DMA_STATUS_BUSY);

    /* No post-invalidate: VRAMPX is write-only from the CPU side. */

    if (status & FPGC_DMA_STATUS_ERROR) {
        return -1;
    }
    return 0;
}
