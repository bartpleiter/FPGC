/*
 * SPI1 DMA bring-up / behavior characterization test.
 *
 * Goal: figure out, on real hardware, exactly which SPI1 access patterns
 * work and which fail. SPI1 is wired to "Flash 2" (BRFS).
 *
 * What we test (in order):
 *   1. JEDEC ID over CPU MMIO  -- SPI0 then SPI1 (sanity: non-DMA path).
 *   2. CPU-loop READ_DATA of 32 bytes from flash address 0x000000 on
 *      SPI0 then SPI1 (slow-path baseline; this is what BDOS used before
 *      the fast path landed).
 *   3. DMA SPI2MEM read of 32 bytes from 0x000000 on SPI0 (sanity:
 *      DMA on the known-good controller).
 *   4. DMA SPI2MEM read of 32 bytes from 0x000000 on SPI1 (the
 *      suspected-broken case).
 *   5. Repeat step 4 with sizes 64, 96, 128 bytes.
 *   6. DMA round-trip on SPI1: read via DMA, compare against the
 *      CPU-loop read of step 2.
 *
 * Each step prints a clear PASS/FAIL line and also dumps the first 8
 * words it observed.
 *
 * NO writes/erases are done -- this is read-only against whatever data
 * happens to be in Flash 2 right now.
 */

#include "spi.h"
#include "spi_flash.h"
#include "dma.h"
#include "uart.h"
#include "fpgc.h"
#include <stdlib.h>

/* Stub for spi_flash.c's reference to BDOS's ENC ISR-defer flag. */
int enc28j60_spi_in_use = 0;

#define SPIFLASH_CMD_READ_DATA 0x03
#define SPIFLASH_DUMMY         0x00
#define SPIFLASH_CMD_JEDEC_ID  0x9F

/* 4 KiB of buffer space (matches BRFS sector size), with room to align
 * to a 32-byte boundary. */
static unsigned int g_buf_storage[1024 + 8];
static unsigned int g_ref_storage[1024 + 8];

static unsigned int *
align32(unsigned int *p)
{
    unsigned int addr = (unsigned int)p;
    unsigned int aligned = (addr + 31u) & ~31u;
    return (unsigned int *)aligned;
}

static void
print_addr(const char *label, void *p)
{
    uart_puts(label);
    uart_puthex((unsigned int)p, 1);
    uart_puts("\n");
}

static void
dump_words(unsigned int *buf, int n)
{
    int i;
    for (i = 0; i < n; i++) {
        uart_puts("    [");
        uart_putint(i);
        uart_puts("] ");
        uart_puthex(buf[i], 1);
        uart_puts("\n");
    }
}

static int
compare_words(unsigned int *a, unsigned int *b, int n)
{
    int i;
    for (i = 0; i < n; i++) {
        if (a[i] != b[i])
            return i; /* index of first mismatch */
    }
    return -1; /* equal */
}

/* CPU-loop read, byte-by-byte via spi_transfer. Mirrors the slow path
 * of spi_flash_read_words. */
static void
cpu_read(int spi_id, int address, unsigned int *buf, int word_count)
{
    int i;
    unsigned int b0, b1, b2, b3;
    spi_select(spi_id);
    spi_transfer(spi_id, SPIFLASH_CMD_READ_DATA);
    spi_transfer(spi_id, (address >> 16) & 0xFF);
    spi_transfer(spi_id, (address >> 8) & 0xFF);
    spi_transfer(spi_id, address & 0xFF);
    for (i = 0; i < word_count; i++) {
        b0 = spi_transfer(spi_id, SPIFLASH_DUMMY);
        b1 = spi_transfer(spi_id, SPIFLASH_DUMMY);
        b2 = spi_transfer(spi_id, SPIFLASH_DUMMY);
        b3 = spi_transfer(spi_id, SPIFLASH_DUMMY);
        buf[i] = b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);
    }
    spi_deselect(spi_id);
}

/* DMA SPI2MEM read. Caller already aligned `buf`. */
static unsigned int
dma_read(int spi_id, int address, unsigned int *buf, int word_count)
{
    unsigned int byte_count = (unsigned int)word_count * 4u;
    unsigned int status;
    spi_select(spi_id);
    spi_transfer(spi_id, SPIFLASH_CMD_READ_DATA);
    spi_transfer(spi_id, (address >> 16) & 0xFF);
    spi_transfer(spi_id, (address >> 8) & 0xFF);
    spi_transfer(spi_id, address & 0xFF);
    cache_flush_data();
    dma_start_spi(DMA_SPI2MEM, spi_id, (unsigned int)buf, 0u, byte_count);
    while (dma_busy())
        ;
    status = dma_status();
    cache_flush_data();
    spi_deselect(spi_id);
    return status;
}

static void
banner(const char *s)
{
    uart_puts("\n--- ");
    uart_puts(s);
    uart_puts(" ---\n");
}

static void
test_jedec(int spi_id)
{
    int mfg = 0, mtype = 0, cap = 0;
    spi_flash_read_jedec_id(spi_id, &mfg, &mtype, &cap);
    uart_puts("  SPI");
    uart_putint(spi_id);
    uart_puts(" JEDEC: mfg=");
    uart_puthex(mfg, 1);
    uart_puts(" type=");
    uart_puthex(mtype, 1);
    uart_puts(" cap=");
    uart_puthex(cap, 1);
    uart_puts("\n");
}

int main(void)
{
    unsigned int *buf = align32(g_buf_storage);
    unsigned int *ref = align32(g_ref_storage);
    unsigned int status;
    int mismatch;
    int word_count;
    int i;

    uart_puts("\n=== SPI1 DMA bring-up test ===\n");
    print_addr("buf @ ", buf);
    print_addr("ref @ ", ref);

    /* Step 1: JEDEC over CPU MMIO. */
    banner("Step 1: JEDEC ID (CPU MMIO)");
    test_jedec(SPI_FLASH_0);
    test_jedec(SPI_FLASH_1);

    /* Step 2: CPU-loop read 32B from 0x000000. */
    banner("Step 2: CPU-loop READ_DATA, 32 bytes @ 0x000000");
    for (i = 0; i < 8; i++) ref[i] = 0xDEADBEEF;
    cpu_read(SPI_FLASH_0, 0, ref, 8);
    uart_puts("  SPI0 first 8 words (CPU):\n");
    dump_words(ref, 8);

    for (i = 0; i < 8; i++) ref[i] = 0xDEADBEEF;
    cpu_read(SPI_FLASH_1, 0, ref, 8);
    uart_puts("  SPI1 first 8 words (CPU):\n");
    dump_words(ref, 8);
    /* Stash SPI1 CPU-read result for later comparison. */
    /* ref now holds the SPI1 CPU baseline. */

    /* Step 3: DMA SPI2MEM 32B on SPI0 (known-good). */
    banner("Step 3: DMA SPI2MEM 32 bytes on SPI0 @ 0x000000");
    for (i = 0; i < 8; i++) buf[i] = 0xDEADBEEF;
    status = dma_read(SPI_FLASH_0, 0, buf, 8);
    uart_puts("  DMA status: ");
    uart_puthex(status, 1);
    uart_puts("\n  SPI0 first 8 words (DMA):\n");
    dump_words(buf, 8);

    /* Step 4: DMA SPI2MEM 32B on SPI1 (the suspect). */
    banner("Step 4: DMA SPI2MEM 32 bytes on SPI1 @ 0x000000");
    for (i = 0; i < 8; i++) buf[i] = 0xDEADBEEF;
    uart_puts("  Issuing DMA SPI2MEM... (will hang here if the path is broken)\n");
    status = dma_read(SPI_FLASH_1, 0, buf, 8);
    uart_puts("  DMA status: ");
    uart_puthex(status, 1);
    uart_puts("\n  SPI1 first 8 words (DMA):\n");
    dump_words(buf, 8);

    /* Step 4b: compare SPI1 DMA vs SPI1 CPU-loop. */
    banner("Step 4b: compare SPI1 DMA vs SPI1 CPU-loop (32B)");
    mismatch = compare_words(buf, ref, 8);
    if (mismatch < 0) {
        uart_puts("  PASS: SPI1 DMA matches CPU-loop read\n");
    } else {
        uart_puts("  FAIL: first mismatch at word ");
        uart_putint(mismatch);
        uart_puts(": dma=");
        uart_puthex(buf[mismatch], 1);
        uart_puts(" cpu=");
        uart_puthex(ref[mismatch], 1);
        uart_puts("\n");
    }

    /* Step 5: SPI1 DMA at larger sizes -- up to a full BRFS sector. */
    {
        int sizes[] = { 16, 32, 64, 128, 256, 512, 1024 };
        int n_sizes = (int)(sizeof(sizes) / sizeof(sizes[0]));
        int s;
        for (s = 0; s < n_sizes; s++) {
            word_count = sizes[s];
            banner("Step 5: SPI1 DMA, varied size");
            uart_puts("  word_count = ");
            uart_putint(word_count);
            uart_puts(" (");
            uart_putint(word_count * 4);
            uart_puts(" bytes)\n");

            for (i = 0; i < word_count; i++) buf[i] = 0xDEADBEEF;
            for (i = 0; i < word_count; i++) ref[i] = 0xDEADBEEF;

            cpu_read(SPI_FLASH_1, 0, ref, word_count);
            uart_puts("  cpu read done\n");
            status = dma_read(SPI_FLASH_1, 0, buf, word_count);
            uart_puts("  DMA status: ");
            uart_puthex(status, 1);
            uart_puts("\n");

            mismatch = compare_words(buf, ref, word_count);
            if (mismatch < 0) {
                uart_puts("  PASS\n");
            } else {
                uart_puts("  FAIL: first mismatch at word ");
                uart_putint(mismatch);
                uart_puts(": dma=");
                uart_puthex(buf[mismatch], 1);
                uart_puts(" cpu=");
                uart_puthex(ref[mismatch], 1);
                uart_puts("\n  DMA first 8 words:\n");
                dump_words(buf, 8);
                uart_puts("  CPU first 8 words:\n");
                dump_words(ref, 8);
            }
        }
    }

    /* Step 6: repeat SPI1 DMA 32B several times to catch flakiness. */
    banner("Step 6: SPI1 DMA 32B x 8 repeats");
    cpu_read(SPI_FLASH_1, 0, ref, 8);
    for (i = 0; i < 8; i++) {
        int j;
        unsigned int *tmp = buf;
        for (j = 0; j < 8; j++) tmp[j] = 0xDEADBEEF;
        status = dma_read(SPI_FLASH_1, 0, tmp, 8);
        mismatch = compare_words(tmp, ref, 8);
        uart_puts("  iter ");
        uart_putint(i);
        uart_puts(" status=");
        uart_puthex(status, 1);
        if (mismatch < 0) {
            uart_puts(" PASS\n");
        } else {
            uart_puts(" FAIL@w");
            uart_putint(mismatch);
            uart_puts(" dma=");
            uart_puthex(tmp[mismatch], 1);
            uart_puts(" cpu=");
            uart_puthex(ref[mismatch], 1);
            uart_puts("\n");
        }
    }

    /* Step 7: BRFS-mount-shaped: 4 consecutive 4-KiB DMA reads at
     * increasing flash offsets. This mirrors what brfs_cache_load does. */
    banner("Step 7: SPI1 DMA 4 KiB x 4 chunks @ 0, 4K, 8K, 12K");
    {
        int chunk;
        unsigned int offset;
        for (chunk = 0; chunk < 4; chunk++) {
            offset = (unsigned int)chunk * 4096u;
            uart_puts("  chunk ");
            uart_putint(chunk);
            uart_puts(" offset=");
            uart_puthex(offset, 1);
            uart_puts("\n");

            for (i = 0; i < 1024; i++) buf[i] = 0xDEADBEEF;
            for (i = 0; i < 1024; i++) ref[i] = 0xDEADBEEF;

            cpu_read(SPI_FLASH_1, (int)offset, ref, 1024);
            status = dma_read(SPI_FLASH_1, (int)offset, buf, 1024);
            uart_puts("    status=");
            uart_puthex(status, 1);
            mismatch = compare_words(buf, ref, 1024);
            if (mismatch < 0) {
                uart_puts(" PASS\n");
            } else {
                uart_puts(" FAIL@w");
                uart_putint(mismatch);
                uart_puts(" dma=");
                uart_puthex(buf[mismatch], 1);
                uart_puts(" cpu=");
                uart_puthex(ref[mismatch], 1);
                uart_puts("\n");
            }
        }
    }

    /* Step 8: Same as Step 7 but using a heap-allocated buffer (mimics
     * BDOS, which uses malloc'd buffers rather than static BSS). */
    banner("Step 8: SPI1 DMA 4 KiB on malloc'd buffer");
    {
        unsigned int *heap_raw;
        unsigned int *heap_buf;
        unsigned int heap_addr;
        heap_raw = (unsigned int *)malloc(1024 * 4 + 64);
        if (heap_raw == 0) {
            uart_puts("  malloc failed\n");
        } else {
            heap_addr = ((unsigned int)heap_raw + 31u) & ~31u;
            heap_buf = (unsigned int *)heap_addr;
            uart_puts("  heap_raw @ ");
            uart_puthex((unsigned int)heap_raw, 1);
            uart_puts("\n  heap_buf @ ");
            uart_puthex((unsigned int)heap_buf, 1);
            uart_puts("\n");

            for (i = 0; i < 1024; i++) heap_buf[i] = 0xDEADBEEF;
            for (i = 0; i < 1024; i++) ref[i] = 0xDEADBEEF;

            cpu_read(SPI_FLASH_1, 0, ref, 1024);
            status = dma_read(SPI_FLASH_1, 0, heap_buf, 1024);
            uart_puts("  status=");
            uart_puthex(status, 1);
            mismatch = compare_words(heap_buf, ref, 1024);
            if (mismatch < 0) {
                uart_puts(" PASS\n");
            } else {
                uart_puts(" FAIL@w");
                uart_putint(mismatch);
                uart_puts(" dma=");
                uart_puthex(heap_buf[mismatch], 1);
                uart_puts(" cpu=");
                uart_puthex(ref[mismatch], 1);
                uart_puts("\n");
            }

            free(heap_raw);
        }
    }

    /* Step 9: SPI1 DMA 4 KiB into a fixed SDRAM address that mirrors what
     * BDOS does at mount (BRFS cache region at 0x3000040). This is the
     * exact transfer that hangs in BDOS. */
    banner("Step 9: SPI1 DMA 4 KiB into fixed SDRAM addr 0x3000040");
    {
        unsigned int *fixed = (unsigned int *)0x3000040u;
        unsigned int j;
        for (j = 0; j < 1024; j++) fixed[j] = 0xDEADBEEF;
        for (j = 0; j < 1024; j++) ref[j] = 0xDEADBEEF;
        cpu_read(SPI_FLASH_1, 0x1000, ref, 1024);
        uart_puts("  cpu read done\n");
        uart_puts("  Issuing DMA SPI2MEM into 0x3000040...\n");
        status = dma_read(SPI_FLASH_1, 0x1000, fixed, 1024);
        uart_puts("  DMA status: ");
        uart_puthex(status, 1);
        uart_puts("\n");
        mismatch = compare_words(fixed, ref, 1024);
        if (mismatch < 0) {
            uart_puts("  PASS\n");
        } else {
            uart_puts("  FAIL@w");
            uart_putint(mismatch);
            uart_puts(" dma=");
            uart_puthex(fixed[mismatch], 1);
            uart_puts(" cpu=");
            uart_puthex(ref[mismatch], 1);
            uart_puts("\n");
        }
    }

    /* Step 10: replicate the EXACT BDOS sequence that hangs:
     *   1) 16-word (64-byte) DMA from flash 0x0 -> SDRAM 0x3000000
     *   2) 1024-word (4 KiB) DMA from flash 0x1000 -> SDRAM 0x3000040
     * (with spi_select / spi_deselect between, like spi_flash_read_words). */
    banner("Step 10: BDOS-style sequence (64B then 4KiB DMA on SPI1)");
    {
        unsigned int *sb_buf  = (unsigned int *)0x3000000u;
        unsigned int *fat_buf = (unsigned int *)0x3000040u;
        unsigned int j;

        for (j = 0; j < 16;   j++) sb_buf[j]  = 0xCAFEBABE;
        for (j = 0; j < 1024; j++) fat_buf[j] = 0xCAFEBABE;

        uart_puts("  step10a: 16-word DMA from 0x0 -> 0x3000000\n");
        status = dma_read(SPI_FLASH_1, 0x0, sb_buf, 16);
        uart_puts("  step10a status="); uart_puthex(status, 1); uart_puts("\n");

        uart_puts("  step10b: 1024-word DMA from 0x1000 -> 0x3000040\n");
        status = dma_read(SPI_FLASH_1, 0x1000, fat_buf, 1024);
        uart_puts("  step10b status="); uart_puthex(status, 1); uart_puts("\n");

        for (j = 0; j < 1024; j++) ref[j] = 0xCAFEBABE;
        cpu_read(SPI_FLASH_1, 0x1000, ref, 1024);
        mismatch = compare_words(fat_buf, ref, 1024);
        if (mismatch < 0) {
            uart_puts("  PASS\n");
        } else {
            uart_puts("  FAIL@w");
            uart_putint(mismatch);
            uart_puts("\n");
        }
    }

    /* Step 11: stress test — do 200 4 KiB DMAs in a row. If any hang,
     * the underlying engine has a race that's exposed under interrupt
     * timing in BDOS. */
    banner("Step 11: stress 200 x 4 KiB SPI1 DMA");
    {
        unsigned int *buf = (unsigned int *)0x3000040u;
        unsigned int j, n;
        for (n = 0; n < 200u; n++) {
            for (j = 0; j < 1024; j++) buf[j] = 0xC001D00D;
            uart_puts("  iter "); uart_putint(n); uart_puts(": start ");
            status = dma_read(SPI_FLASH_1, 0x1000, buf, 1024);
            uart_puts("done st=");
            uart_puthex(status, 1);
            uart_puts("\n");
        }
        uart_puts("  Step 11 complete (no hang)\n");
    }

    uart_puts("\n=== Test complete ===\n");
    return 0;
}

void interrupt(void)
{
}
