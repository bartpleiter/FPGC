#ifndef FPGC_BRFS_STORAGE_SPI_FLASH_H
#define FPGC_BRFS_STORAGE_SPI_FLASH_H

#include "brfs_storage.h"

/*
 * SPI-flash backend for BRFS v2 storage layer.
 *
 * Embeds `brfs_storage_t` as its first member so a pointer to this
 * struct (or its `base` field) can be passed wherever a generic
 * `brfs_storage_t *` is expected.
 */

typedef struct {
    brfs_storage_t base;   /* MUST be first member */
    int            flash_id;
} brfs_spi_flash_storage_t;

/* Initialise `out` to talk to the SPI flash on `flash_id` (e.g.
 * SPI_FLASH_1). Idempotent; performs no I/O. */
void brfs_storage_spi_flash_init(brfs_spi_flash_storage_t *out, int flash_id);

#endif /* FPGC_BRFS_STORAGE_SPI_FLASH_H */
