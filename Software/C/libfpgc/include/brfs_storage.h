#ifndef FPGC_BRFS_STORAGE_H
#define FPGC_BRFS_STORAGE_H

/*
 * BRFS storage backend abstraction.
 *
 * Phase 1 of the BRFS v2 plan (Docs/plans/BRFS-v2.md). The vtable is
 * intentionally word-addressed for now to mirror the v1 BRFS API
 * exactly — this layer is a behaviour-preserving refactor whose only
 * job is to remove the hard dependency on `spi_flash_*` from brfs.c.
 *
 * Phase 2 will add block-oriented `read_block` / `write_block`
 * methods (4 KiB granularity) used by the LRU cache. Phase 5 will
 * remove the word-addressed methods once nothing in BRFS calls them
 * directly anymore.
 *
 * Concrete backends "inherit" by embedding `brfs_storage_t` as the
 * first member of their own struct and casting `self` back to it. See
 * brfs_storage_spi_flash.h for an example.
 */

struct brfs_storage;

typedef struct brfs_storage {
    /* Read `n_words` 32-bit words starting at byte address `addr`.
     * Returns 0 on success, <0 on error. */
    int (*read_words)(struct brfs_storage *self,
                      unsigned int addr,
                      unsigned int *dst,
                      unsigned int n_words);

    /* Write `n_words` 32-bit words starting at byte address `addr`.
     * The caller is responsible for having erased the affected sector
     * first (NAND/NOR semantics). Returns 0 on success, <0 on error. */
    int (*write_words)(struct brfs_storage *self,
                       unsigned int addr,
                       const unsigned int *src,
                       unsigned int n_words);

    /* Erase the sector containing byte address `addr` (4 KiB granular).
     * Returns 0 on success, <0 on error. */
    int (*erase_sector)(struct brfs_storage *self,
                        unsigned int addr);
} brfs_storage_t;

#endif /* FPGC_BRFS_STORAGE_H */
