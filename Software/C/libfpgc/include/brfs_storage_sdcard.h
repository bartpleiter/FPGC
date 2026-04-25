#ifndef FPGC_BRFS_STORAGE_SDCARD_H
#define FPGC_BRFS_STORAGE_SDCARD_H

#include "brfs_storage.h"

/*
 * SD card backend for BRFS v2 storage layer.
 *
 * Embeds `brfs_storage_t` as its first member so a pointer to this
 * struct (or its `base` field) can be passed wherever a generic
 * `brfs_storage_t *` is expected.
 *
 * The SD driver is block-addressed (512 B logical blocks); this
 * wrapper translates the BRFS byte-addressed / word-granular API
 * onto sd_read_block / sd_write_block. erase_sector is a no-op
 * (SD cards have no host-visible erase requirement; the controller's
 * FTL handles it).
 *
 * Caller must run sd_init() before any BRFS operation on this
 * backend; brfs_storage_sdcard_init does no I/O.
 */

typedef struct {
    brfs_storage_t base;   /* MUST be first member */
} brfs_sdcard_storage_t;

void brfs_storage_sdcard_init(brfs_sdcard_storage_t *out);

#endif /* FPGC_BRFS_STORAGE_SDCARD_H */
