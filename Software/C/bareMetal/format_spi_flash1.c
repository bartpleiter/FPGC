/*
 * format_spi_flash1.c — Bare-metal BRFS formatter for SPI Flash 1.
 *
 * Initializes SPI, formats a BRFS v2 filesystem on SPI Flash 1
 * (the root filesystem chip), and halts.
 *
 * Parameters match the standard BDOS format command:
 *   4032 blocks × 1024 words/block (4096 bytes/block) = ~16 MiB
 *   Label: "brfs"
 *
 * Usage:
 *   make run-c-baremetal-uart file=format_spi_flash1
 */

#include "uart.h"
#include "fpgc.h"
#include "spi_flash.h"
#include "brfs.h"
#include "brfs_storage_spi_flash.h"

/* Format parameters — same as "format 4032 4096 brfs" */
#define FORMAT_BLOCKS         4032
#define FORMAT_WORDS_PER_BLK  1024   /* 4096 bytes per block */
#define FORMAT_LABEL          "brfs"

/*
 * BRFS cache region — placed well above the program code.
 * 28 MiB starting at 0x2400000 (same layout as the kernel).
 */
#define CACHE_START  0x2400000
#define CACHE_SIZE   ((0x4000000 - 0x2400000) / 4)  /* in words */

static struct brfs_state          brfs;
static brfs_spi_flash_storage_t   storage;

static void progress(const char *phase, unsigned int done, unsigned int total)
{
    /* Simple progress: print a dot every 64 blocks */
    if ((done & 63) == 0)
        uart_puts(".");
}

void interrupt(void)
{
    /* No interrupts used */
}

int main(void)
{
    int result;

    uart_init();

    uart_puts("\n=== SPI Flash 1 BRFS Formatter ===\n");
    uart_puts("Blocks:          4032\n");
    uart_puts("Bytes per block: 4096\n");
    uart_puts("Label:           brfs\n\n");

    /* Init storage backend for SPI Flash 1 */
    brfs_storage_spi_flash_init(&storage, FPGC_SPI_FLASH_1);

    /* Init BRFS state with cache */
    result = brfs_init(&brfs, &storage.base,
                       (unsigned int *)CACHE_START, CACHE_SIZE);
    if (result != BRFS_OK)
    {
        uart_puts("ERROR: brfs_init failed: ");
        uart_puthex(result, 1);
        uart_puts("\n");
        return 1;
    }

    /* Set progress callback */
    brfs_set_progress_callback(&brfs, progress);

    /* Format (full format — zeroes all blocks) */
    uart_puts("Formatting");
    result = brfs_format(&brfs, FORMAT_BLOCKS, FORMAT_WORDS_PER_BLK,
                         FORMAT_LABEL, 1);
    uart_puts("\n");

    if (result != BRFS_OK)
    {
        uart_puts("ERROR: brfs_format failed: ");
        uart_puthex(result, 1);
        uart_puts("\n");
        return 2;
    }

    /* Sync to flash — brfs_format only writes to cache */
    uart_puts("Syncing to flash...\n");
    result = brfs_sync(&brfs);
    if (result != BRFS_OK)
    {
        uart_puts("ERROR: brfs_sync failed: ");
        uart_puthex(result, 1);
        uart_puts("\n");
        return 3;
    }

    uart_puts("Format + sync complete!\n");

    /* Verify by mounting */
    result = brfs_mount(&brfs);
    if (result != BRFS_OK)
    {
        uart_puts("WARNING: mount verification failed: ");
        uart_puthex(result, 1);
        uart_puts("\n");
        return 4;
    }

    uart_puts("Mount verification OK.\n");
    uart_puts("=== Done. Flash kernel next. ===\n");

    return 0;
}
