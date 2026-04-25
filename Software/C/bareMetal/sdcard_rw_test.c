/*
 * SD card single-block read/write smoke test (B.5.2).
 *
 * Sequence:
 *   1. sd_init() -- prints OCR, capacity (in MB).
 *   2. Read sector 0 (MBR / boot sector) and dump the first 64 bytes
 *      hex over UART. User can compare to host-side `dd if=/dev/sdX
 *      bs=512 count=1 | xxd | head -4`.
 *   3. Read sector 1, then *write* a known pattern to sector 1
 *      (CMD24), then read sector 1 again and verify the pattern
 *      round-trips. Restores the original sector 1 contents at the
 *      end so the test is non-destructive.
 *
 * SECTOR 1 IS USUALLY EMPTY ON A FAT-FORMATTED CARD (the MBR lives
 * in sector 0 and the partition starts further in). If your card is
 * partitioned with sector 1 in use (rare on microSD), edit
 * TEST_LBA below before running.
 */

#include "uart.h"
#include "sd.h"

#define TEST_LBA 1

/* 32-byte aligned to take the DMA fast path in sd_read_block /
 * sd_write_block (B.5.3). */
static _Alignas(32) unsigned char buf_a[SD_BLOCK_SIZE];
static _Alignas(32) unsigned char buf_b[SD_BLOCK_SIZE];
static _Alignas(32) unsigned char buf_save[SD_BLOCK_SIZE];

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
dump64(const unsigned char *b)
{
    int i;
    for (i = 0; i < 64; i++) {
        if ((i & 15) == 0) {
            uart_puts("\n  ");
            uart_puthex(i, 1);
            uart_puts(": ");
        }
        uart_puthex(b[i], 0);
        uart_puts(" ");
    }
    uart_puts("\n");
}

int main(void)
{
    sd_card_info_t info;
    sd_result_t r;
    int i;
    int mismatches;

    uart_puts("\n=== SD card B.5.2: read+write smoke test ===\n");

    r = sd_init(&info);
    uart_puts("sd_init -> ");
    uart_puts(sd_err_name(r));
    uart_puts("\n");
    if (r != SD_OK) { while (1) ; }

    uart_puts("blocks=");
    uart_putint(info.blocks);
    uart_puts(" (~");
    uart_putint(info.blocks / 2048);   /* MB = blocks * 512 / 1MB */
    uart_puts(" MB)\n");

    /* --- Sector 0 dump --- */
    uart_puts("\nReading LBA 0 (MBR/boot)...");
    r = sd_read_block(0, buf_a);
    uart_puts(" -> "); uart_puts(sd_err_name(r)); uart_puts("\n");
    if (r != SD_OK) { while (1) ; }
    uart_puts("First 64 bytes:");
    dump64(buf_a);
    uart_puts("Last 4 bytes (expect 55 AA at offsets 510-511 if MBR): ");
    uart_puthex(buf_a[508], 0); uart_puts(" ");
    uart_puthex(buf_a[509], 0); uart_puts(" ");
    uart_puthex(buf_a[510], 0); uart_puts(" ");
    uart_puthex(buf_a[511], 0); uart_puts("\n");

    /* --- Sector TEST_LBA round-trip --- */
    uart_puts("\nRead LBA "); uart_putint(TEST_LBA); uart_puts(" (save)...");
    r = sd_read_block(TEST_LBA, buf_save);
    uart_puts(" -> "); uart_puts(sd_err_name(r)); uart_puts("\n");
    if (r != SD_OK) { while (1) ; }

    /* Build a recognisable pattern: 'F','P','G','C','0' .. '7' rotated. */
    for (i = 0; i < SD_BLOCK_SIZE; i++)
        buf_a[i] = (unsigned char)((i * 31u + 0xA5u) & 0xFF);

    uart_puts("Write LBA "); uart_putint(TEST_LBA); uart_puts(" (pattern)...");
    r = sd_write_block(TEST_LBA, buf_a);
    uart_puts(" -> "); uart_puts(sd_err_name(r)); uart_puts("\n");
    if (r != SD_OK) {
        uart_puts("Write failed -- not attempting restore.\n");
        while (1) ;
    }

    uart_puts("Read back LBA "); uart_putint(TEST_LBA); uart_puts("...");
    r = sd_read_block(TEST_LBA, buf_b);
    uart_puts(" -> "); uart_puts(sd_err_name(r)); uart_puts("\n");
    if (r != SD_OK) { while (1) ; }

    mismatches = 0;
    for (i = 0; i < SD_BLOCK_SIZE; i++) {
        if (buf_a[i] != buf_b[i])
            mismatches++;
    }
    uart_puts("Byte mismatches: ");
    uart_putint(mismatches);
    uart_puts(mismatches == 0 ? "  -- PASS\n" : "  -- FAIL\n");
    if (mismatches != 0) {
        uart_puts("Wrote first 16: ");
        for (i = 0; i < 16; i++) { uart_puthex(buf_a[i], 0); uart_puts(" "); }
        uart_puts("\nGot   first 16: ");
        for (i = 0; i < 16; i++) { uart_puthex(buf_b[i], 0); uart_puts(" "); }
        uart_puts("\n");
    }

    /* Restore. */
    uart_puts("Restoring LBA "); uart_putint(TEST_LBA); uart_puts("...");
    r = sd_write_block(TEST_LBA, buf_save);
    uart_puts(" -> "); uart_puts(sd_err_name(r)); uart_puts("\n");

    uart_puts("=== Done ===\n");
    while (1) ;
    return 0;
}

void interrupt(void) { }
