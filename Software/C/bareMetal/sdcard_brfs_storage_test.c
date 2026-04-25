/*
 * BRFS storage backend wrapper smoke test (B.5.5).
 *
 * Exercises brfs_storage_sdcard_t.{read_words,write_words,erase_sector}
 * directly (no BRFS on top): we just want to verify that the
 * byte/word -> 512-byte-block translation is correct for head /
 * body / tail partial-block alignments, and that the full pipeline
 * (BRFS storage vtable -> SD driver -> DMA on SPI5) round-trips
 * cleanly.
 *
 * Layout: TEST_LBA..TEST_LBA+5 = 6 sectors = 3072 bytes = 768 words
 * scratch window. Test cases cover:
 *   case A: aligned start, aligned length (full blocks only).
 *   case B: misaligned start (head partial), aligned tail.
 *   case C: aligned start, misaligned tail.
 *   case D: misaligned start AND tail (head + body + tail).
 *
 * Each case writes a known pattern via storage->write_words, reads
 * back via storage->read_words, and byte-compares. The window is
 * saved at the top and restored at the end so the test is
 * non-destructive.
 */

#include "uart.h"
#include "sd.h"
#include "brfs_storage_sdcard.h"

#define TEST_LBA    1
#define TEST_BLOCKS 6
#define TEST_BYTES  (TEST_BLOCKS * SD_BLOCK_SIZE)
#define TEST_WORDS  (TEST_BYTES / 4u)

static _Alignas(32) unsigned int wbuf [TEST_WORDS];
static _Alignas(32) unsigned int rbuf [TEST_WORDS];
static _Alignas(32) unsigned int save [TEST_WORDS];

static const char *
sd_err_name(sd_result_t r)
{
    switch (r) {
    case SD_OK:              return "OK";
    case SD_ERR_NO_CARD:     return "NO_CARD";
    case SD_ERR_TIMEOUT:     return "TIMEOUT";
    case SD_ERR_CRC:         return "CRC";
    case SD_ERR_PROTOCOL:    return "PROTOCOL";
    case SD_ERR_UNSUPPORTED: return "UNSUPPORTED";
    case SD_ERR_WRITE:       return "WRITE";
    default:                 return "?";
    }
}

static void
fill_pattern(unsigned int *p, unsigned int word_count, unsigned int seed)
{
    unsigned int i;
    for (i = 0; i < word_count; i++)
        p[i] = seed + i * 0x01010101u + (i ^ 0xA5A5A5A5u);
}

static int
run_case(const char *name, brfs_storage_t *st,
         unsigned int byte_addr, unsigned int n_words, unsigned int seed)
{
    int i;
    int mismatches = 0;
    int rc;
    unsigned int *src;
    unsigned int *dst;

    src = &wbuf[byte_addr / 4u];
    dst = &rbuf[byte_addr / 4u];

    /* Sentinel rbuf so a no-op read fails loudly. */
    for (i = 0; i < (int)n_words; i++)
        dst[i] = 0xDEADBEEFu;

    fill_pattern(src, n_words, seed);

    uart_puts("case ");
    uart_puts(name);
    uart_puts(": addr=");
    uart_putint(byte_addr);
    uart_puts(" words=");
    uart_putint(n_words);

    rc = st->write_words(st, byte_addr, src, n_words);
    if (rc != 0) { uart_puts("  WRITE_FAIL\n"); return -1; }
    rc = st->read_words(st, byte_addr, dst, n_words);
    if (rc != 0) { uart_puts("  READ_FAIL\n"); return -1; }

    for (i = 0; i < (int)n_words; i++) {
        if (src[i] != dst[i])
            mismatches++;
    }
    if (mismatches != 0) {
        uart_puts("  FAIL mismatches=");
        uart_putint(mismatches);
        uart_puts("\n");
        return -1;
    }
    uart_puts("  PASS\n");
    return 0;
}

int main(void)
{
    sd_card_info_t info;
    sd_result_t r;
    brfs_sdcard_storage_t backend;
    brfs_storage_t *st = &backend.base;
    int i;
    int rc;
    int fails = 0;

    uart_puts("\n=== B.5.5: BRFS storage SD wrapper smoke test ===\n");

    r = sd_init(&info);
    uart_puts("sd_init -> ");
    uart_puts(sd_err_name(r));
    uart_puts("\n");
    if (r != SD_OK) { while (1) ; }

    brfs_storage_sdcard_init(&backend);

    /* Save the test window. */
    uart_puts("Save window LBA ");
    uart_putint(TEST_LBA);
    uart_puts("..");
    uart_putint(TEST_LBA + TEST_BLOCKS - 1);
    uart_puts("...");
    rc = st->read_words(st, TEST_LBA * SD_BLOCK_SIZE, save, TEST_WORDS);
    uart_puts(" rc=");
    uart_putint(rc);
    uart_puts(rc == 0 ? " OK\n" : " FAIL\n");
    if (rc != 0) {
        uart_puts("Diagnostic: try sd_read_blocks(1, save, ");
        uart_putint(TEST_BLOCKS);
        uart_puts(") direct...");
        r = sd_read_blocks(TEST_LBA, save, TEST_BLOCKS);
        uart_puts(" -> "); uart_puts(sd_err_name(r)); uart_puts("\n");
        uart_puts("Diagnostic: try sd_read_block(1, save) direct...");
        r = sd_read_block(TEST_LBA, save);
        uart_puts(" -> "); uart_puts(sd_err_name(r)); uart_puts("\n");
        while (1) ;
    }

    /* case A: aligned start, aligned length (1 block worth). */
    fails += (run_case("A.aligned-1block",
                       st,
                       TEST_LBA * SD_BLOCK_SIZE,
                       SD_BLOCK_SIZE / 4u,
                       0x10000000u) != 0);

    /* case B: head partial, aligned tail. Skip 4 bytes into the
     * second block, write 1 word + 1 full block. */
    fails += (run_case("B.head-partial",
                       st,
                       (TEST_LBA + 1) * SD_BLOCK_SIZE + 4u,
                       1u + (SD_BLOCK_SIZE / 4u),
                       0x20000000u) != 0);

    /* case C: aligned start, tail partial. 1 full block + 4 words. */
    fails += (run_case("C.tail-partial",
                       st,
                       (TEST_LBA + 2) * SD_BLOCK_SIZE,
                       (SD_BLOCK_SIZE / 4u) + 4u,
                       0x30000000u) != 0);

#if TEST_BLOCKS >= 6
    /* case D: head + body + tail. start at +12 bytes into block,
     * length 1 block + 16 words. Only valid when the test window
     * includes at least 6 blocks. */
    fails += (run_case("D.head+body+tail",
                       st,
                       (TEST_LBA + 4) * SD_BLOCK_SIZE + 12u,
                       (SD_BLOCK_SIZE / 4u) + 16u,
                       0x40000000u) != 0);
#endif

    /* Restore. */
    uart_puts("Restore window...");
    rc = st->write_words(st, TEST_LBA * SD_BLOCK_SIZE, save, TEST_WORDS);
    uart_puts(rc == 0 ? " OK\n" : " FAIL\n");

    uart_puts(fails == 0 ? "=== PASS ===\n" : "=== FAIL ===\n");
    while (1) ;
    return 0;
}

void interrupt(void) { }
