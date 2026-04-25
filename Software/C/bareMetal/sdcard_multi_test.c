/*
 * SD card multi-block read/write smoke test (B.5.4).
 *
 * Sequence:
 *   1. sd_init() -- print capacity.
 *   2. Save 4 sectors starting at TEST_LBA via sd_read_blocks (CMD18).
 *   3. Build a recognisable per-block pattern, write 4 blocks via
 *      sd_write_blocks (CMD25 + STOP_TRAN), read them back with
 *      sd_read_blocks, verify byte-for-byte match.
 *   4. Restore the original 4 blocks via sd_write_blocks.
 *
 * Non-destructive: the original 4-sector window is restored at the
 * end. Edit TEST_LBA below if your card has those sectors in use.
 */

#include "uart.h"
#include "sd.h"

#define TEST_LBA   1
#define TEST_BLOCKS 4
#define TEST_BYTES (TEST_BLOCKS * SD_BLOCK_SIZE)

/* 32-byte aligned to take the DMA fast path. */
static _Alignas(32) unsigned char buf_a   [TEST_BYTES];
static _Alignas(32) unsigned char buf_b   [TEST_BYTES];
static _Alignas(32) unsigned char buf_save[TEST_BYTES];

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

int main(void)
{
    sd_card_info_t info;
    sd_result_t r;
    unsigned int i;
    unsigned int blk;
    int mismatches;
    int blk_with_diff;

    uart_puts("\n=== SD card B.5.4: multi-block smoke test ===\n");

    r = sd_init(&info);
    uart_puts("sd_init -> ");
    uart_puts(sd_err_name(r));
    uart_puts("\n");
    if (r != SD_OK) { while (1) ; }

    uart_puts("blocks=");
    uart_putint(info.blocks);
    uart_puts(" (~");
    uart_putint(info.blocks / 2048);
    uart_puts(" MB)\n");

    /* --- Save TEST_BLOCKS sectors starting at TEST_LBA --- */
    uart_puts("\nSave LBA ");
    uart_putint(TEST_LBA);
    uart_puts("..");
    uart_putint(TEST_LBA + TEST_BLOCKS - 1);
    uart_puts(" via CMD18...");
    r = sd_read_blocks(TEST_LBA, buf_save, TEST_BLOCKS);
    uart_puts(" -> "); uart_puts(sd_err_name(r)); uart_puts("\n");
    if (r != SD_OK) { while (1) ; }

    /* Build a per-block pattern: byte = (block_index * 0x10) ^ (i * 31 + 0xA5).
     * Each block is distinct so a misordered read is loud. */
    for (blk = 0; blk < TEST_BLOCKS; blk++) {
        unsigned char *p = &buf_a[blk * SD_BLOCK_SIZE];
        unsigned char tag = (unsigned char)(blk * 0x10);
        for (i = 0; i < SD_BLOCK_SIZE; i++)
            p[i] = (unsigned char)(tag ^ ((i * 31u + 0xA5u) & 0xFF));
    }

    uart_puts("Write ");
    uart_putint(TEST_BLOCKS);
    uart_puts(" blocks via CMD25...");
    r = sd_write_blocks(TEST_LBA, buf_a, TEST_BLOCKS);
    uart_puts(" -> "); uart_puts(sd_err_name(r)); uart_puts("\n");
    if (r != SD_OK) {
        uart_puts("Write failed -- not attempting restore.\n");
        while (1) ;
    }

    uart_puts("Read back ");
    uart_putint(TEST_BLOCKS);
    uart_puts(" blocks via CMD18...");
    r = sd_read_blocks(TEST_LBA, buf_b, TEST_BLOCKS);
    uart_puts(" -> "); uart_puts(sd_err_name(r)); uart_puts("\n");
    if (r != SD_OK) { while (1) ; }

    mismatches = 0;
    blk_with_diff = -1;
    for (i = 0; i < TEST_BYTES; i++) {
        if (buf_a[i] != buf_b[i]) {
            if (blk_with_diff < 0)
                blk_with_diff = (int)(i / SD_BLOCK_SIZE);
            mismatches++;
        }
    }
    uart_puts("Byte mismatches: ");
    uart_putint(mismatches);
    uart_puts(mismatches == 0 ? "  -- PASS\n" : "  -- FAIL\n");
    if (mismatches != 0) {
        uart_puts("First diff at block #");
        uart_putint(blk_with_diff);
        uart_puts("\n");
    }

    /* Restore. */
    uart_puts("Restore via CMD25...");
    r = sd_write_blocks(TEST_LBA, buf_save, TEST_BLOCKS);
    uart_puts(" -> "); uart_puts(sd_err_name(r)); uart_puts("\n");

    uart_puts("=== Done ===\n");
    while (1) ;
    return 0;
}

void interrupt(void) { }
