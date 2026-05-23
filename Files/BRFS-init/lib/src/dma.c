/*
 * dma.c — userlib DMA driver (mirrors libfpgc/io/dma.c).
 */

#include <dma.h>

int
dma_busy(void)
{
    return (int)((unsigned int)__builtin_load(FPGC_DMA_STATUS)
                 & FPGC_DMA_STATUS_BUSY);
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

int
dma_copy(unsigned int dst, unsigned int src, unsigned int count)
{
    unsigned int status;

    cache_flush_data();
    dma_start_mem2mem(dst, src, count);
    do { status = dma_status(); } while (status & FPGC_DMA_STATUS_BUSY);
    cache_flush_data();
    if (status & FPGC_DMA_STATUS_ERROR) return -1;
    return 0;
}

int
dma_blit_to_vram(unsigned int dst, unsigned int src, unsigned int count)
{
    unsigned int status;

    cache_flush_data();
    dma_start_mem2vram(dst, src, count);
    do { status = dma_status(); } while (status & FPGC_DMA_STATUS_BUSY);
    /* No post-invalidate: VRAMPX is write-only from the CPU side. */
    if (status & FPGC_DMA_STATUS_ERROR) return -1;
    return 0;
}
