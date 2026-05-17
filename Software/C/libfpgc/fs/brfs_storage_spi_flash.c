/*
 * BRFS storage backend for SPI NOR flash
 *
 * Implements the brfs_storage_t vtable for SPI flash chips.
 * Used by the root filesystem (/).
 *
 * Bus: SPI0 (FPGC_SPI_FLASH_0) or SPI1 (FPGC_SPI_FLASH_1)
 *
 * Dependencies: spi_flash.h, brfs_storage_spi_flash.h
 * Build: part of libfpgc (make compile-kernel)
 */
#include "brfs_storage_spi_flash.h"
#include "spi_flash.h"

static int spi_read_words(brfs_storage_t *self,
                          unsigned int addr,
                          unsigned int *dst,
                          unsigned int n_words)
{
    brfs_spi_flash_storage_t *s = (brfs_spi_flash_storage_t *)self;
    spi_flash_read_words(s->flash_id, (int)addr, dst, (int)n_words);
    return 0;
}

static int spi_write_words(brfs_storage_t *self,
                           unsigned int addr,
                           const unsigned int *src,
                           unsigned int n_words)
{
    brfs_spi_flash_storage_t *s = (brfs_spi_flash_storage_t *)self;
    /* spi_flash_write_words takes a non-const pointer for legacy reasons.
     * It does not modify the buffer, so cast away const safely. */
    spi_flash_write_words(s->flash_id, (int)addr,
                          (unsigned int *)src, (int)n_words);
    return 0;
}

static int spi_erase_sector(brfs_storage_t *self, unsigned int addr)
{
    brfs_spi_flash_storage_t *s = (brfs_spi_flash_storage_t *)self;
    spi_flash_erase_sector(s->flash_id, (int)addr);
    return 0;
}

void brfs_storage_spi_flash_init(brfs_spi_flash_storage_t *out, int flash_id)
{
    out->flash_id          = flash_id;
    out->base.read_words   = spi_read_words;
    out->base.write_words  = spi_write_words;
    out->base.erase_sector = spi_erase_sector;
}
