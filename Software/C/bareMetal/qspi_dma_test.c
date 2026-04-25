/*
 * SPI1 QSPI Fast Read DMA bring-up test.
 *
 * Validates the new MODE_SPI2MEM_QSPI (mode=6) path against the real
 * W25Q-style flash on the BRFS port (SPI1). We compare bytes returned
 * by the DMA-driven Quad I/O Fast Read (opcode 0xEB) against a
 * known-good 1-bit CPU-loop READ_DATA (opcode 0x03) of the same
 * address.
 *
 * Sequence:
 *   1. JEDEC ID over CPU MMIO -- sanity that 1-bit IO0 is alive.
 *   2. CPU-loop READ_DATA reference of N bytes from flash @ 0.
 *   3. DMA QSPI Fast Read of the same range -> compare.
 *   4. Repeat at non-zero offsets and varied sizes.
 *   5. Repeat the BRFS-mount-shape: 4x 4 KiB chunks at 0/4K/8K/12K.
 *   6. Run a few back-to-back QSPI bursts to exercise the
 *      `q_continuous` skip-opcode path.
 *
 * NO writes/erases are performed.
 *
 * Build: `make compile-spi1-qspi-test` (target added in the top-level
 * Makefile alongside compile-spi1-dma-test).
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
            return i;
    }
    return -1;
}

/* CPU-loop 1-bit read, byte-by-byte. */
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

/* DMA QSPI Fast Read. The DMA engine is constrained to a single 32-byte
 * line per call (the W25Q does not accept a fresh opcode mid-transaction
 * and the engine doesn't drive CS). Software loops here in 32-byte chunks
 * with spi_select/spi_deselect around each chunk. Returns the OR of all
 * per-chunk status words. */
static unsigned int
qspi_dma_read(int spi_id, unsigned int address, unsigned int *buf,
              int word_count)
{
    unsigned int status_acc = 0;
    int          words_left = word_count;
    int          words_off  = 0;
    while (words_left > 0) {
        int chunk_words = words_left > 8 ? 8 : words_left;
        unsigned int chunk_bytes = (unsigned int)chunk_words * 4u;
        spi_select(spi_id);
        cache_flush_data();
        dma_start_spi_qspi_read(spi_id,
                                (unsigned int)(buf + words_off),
                                address + (unsigned int)(words_off * 4),
                                chunk_bytes);
        while (dma_busy())
            ;
        status_acc |= dma_status();
        cache_flush_data();
        spi_deselect(spi_id);
        words_off  += chunk_words;
        words_left -= chunk_words;
    }
    return status_acc;
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

static void
compare_and_report(unsigned int *dma_buf, unsigned int *ref_buf,
                   int word_count, const char *label)
{
    int mismatch = compare_words(dma_buf, ref_buf, word_count);
    if (mismatch < 0) {
        uart_puts("  PASS: ");
        uart_puts(label);
        uart_puts("\n");
    } else {
        uart_puts("  FAIL: ");
        uart_puts(label);
        uart_puts(": first mismatch at word ");
        uart_putint(mismatch);
        uart_puts(": qspi=");
        uart_puthex(dma_buf[mismatch], 1);
        uart_puts(" cpu=");
        uart_puthex(ref_buf[mismatch], 1);
        uart_puts("\n  QSPI first 8 words:\n");
        dump_words(dma_buf, 8);
        uart_puts("  CPU  first 8 words:\n");
        dump_words(ref_buf, 8);
    }
}

int main(void)
{
    unsigned int *buf = align32(g_buf_storage);
    unsigned int *ref = align32(g_ref_storage);
    unsigned int status;
    int word_count;
    int i;

    uart_puts("\n=== SPI1 QSPI DMA bring-up test ===\n");
    print_addr("buf @ ", buf);
    print_addr("ref @ ", ref);

    /* Step 1: JEDEC over CPU MMIO. */
    banner("Step 1: JEDEC ID (CPU MMIO, 1-bit)");
    test_jedec(SPI_FLASH_0);
    test_jedec(SPI_FLASH_1);

    /* Step 2: CPU-loop reference read of 32 bytes from 0x000000. */
    banner("Step 2: CPU-loop READ_DATA reference, 32 bytes @ 0");
    for (i = 0; i < 8; i++) ref[i] = 0xDEADBEEF;
    cpu_read(SPI_FLASH_1, 0, ref, 8);
    uart_puts("  SPI1 first 8 words (CPU 1-bit):\n");
    dump_words(ref, 8);

    /* Step 3: DMA QSPI Fast Read 32 bytes from 0x000000. */
    banner("Step 3: DMA QSPI Fast Read, 32 bytes @ 0");
    for (i = 0; i < 8; i++) buf[i] = 0xDEADBEEF;
    uart_puts("  Issuing DMA QSPI read... (will hang here if path is broken)\n");
    status = qspi_dma_read(SPI_FLASH_1, 0u, buf, 8);
    uart_puts("  DMA status: ");
    uart_puthex(status, 1);
    uart_puts("\n  SPI1 first 8 words (QSPI DMA):\n");
    dump_words(buf, 8);
    compare_and_report(buf, ref, 8, "32B @ 0 vs CPU 1-bit");

    /* Step 4: varied sizes at offset 0. */
    {
        int sizes[] = { 16, 32, 64, 128, 256, 512, 1024 };
        int n_sizes = (int)(sizeof(sizes) / sizeof(sizes[0]));
        int s;
        for (s = 0; s < n_sizes; s++) {
            word_count = sizes[s];
            banner("Step 4: QSPI DMA, varied size @ 0");
            uart_puts("  word_count = ");
            uart_putint(word_count);
            uart_puts(" (");
            uart_putint(word_count * 4);
            uart_puts(" bytes)\n");

            for (i = 0; i < word_count; i++) buf[i] = 0xDEADBEEF;
            for (i = 0; i < word_count; i++) ref[i] = 0xDEADBEEF;

            cpu_read(SPI_FLASH_1, 0, ref, word_count);
            status = qspi_dma_read(SPI_FLASH_1, 0u, buf, word_count);
            uart_puts("  status=");
            uart_puthex(status, 1);
            uart_puts("\n");
            compare_and_report(buf, ref, word_count, "size sweep");
        }
    }

    /* Step 5: BRFS-mount-shape -- 4x 4 KiB at 0/4K/8K/12K. */
    banner("Step 5: QSPI DMA 4 KiB x 4 chunks @ 0, 4K, 8K, 12K");
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
            status = qspi_dma_read(SPI_FLASH_1, offset, buf, 1024);
            uart_puts("    status=");
            uart_puthex(status, 1);
            uart_puts("\n");
            compare_and_report(buf, ref, 1024, "4 KiB chunk");
        }
    }

    /* Step 6: back-to-back 32B QSPI bursts (exercises q_continuous). */
    banner("Step 6: QSPI DMA 32B x 8 repeats");
    cpu_read(SPI_FLASH_1, 0, ref, 8);
    for (i = 0; i < 8; i++) {
        int j;
        for (j = 0; j < 8; j++) buf[j] = 0xDEADBEEF;
        status = qspi_dma_read(SPI_FLASH_1, 0u, buf, 8);
        uart_puts("  iter ");
        uart_putint(i);
        uart_puts(" status=");
        uart_puthex(status, 1);
        if (compare_words(buf, ref, 8) < 0) {
            uart_puts(" PASS\n");
        } else {
            int mismatch = compare_words(buf, ref, 8);
            uart_puts(" FAIL@w");
            uart_putint(mismatch);
            uart_puts(" qspi=");
            uart_puthex(buf[mismatch], 1);
            uart_puts(" cpu=");
            uart_puthex(ref[mismatch], 1);
            uart_puts("\n");
        }
    }

    uart_puts("\n=== Done ===\n");
    while (1) {
        /* halt */
    }
    return 0;
}

void interrupt(void)
{
}
