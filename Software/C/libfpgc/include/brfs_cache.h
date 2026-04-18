#ifndef FPGC_BRFS_CACHE_H
#define FPGC_BRFS_CACHE_H

#include "brfs_storage.h"

/*
 * BRFS in-RAM block cache (Phase 2 of BRFS v2 — see Docs/plans/BRFS-v2.md).
 *
 * Today the cache is a single linear buffer that mirrors the entire
 * on-disk image: [superblock | FAT | data]. The cache module owns the
 * buffer, the per-FS-block dirty bitmap, and the load/flush traffic
 * against the storage backend. BRFS file/dir logic talks only to the
 * cache API and never to the storage backend directly.
 *
 * Phase 4 will swap the linear buffer for an LRU pool of fixed-size
 * slots (each one storage sector) that can hold an FS larger than
 * the cache itself. The API below is designed so that swap is local
 * to brfs_cache.c — `brfs_cache_data(blk)` returns a pointer that is
 * guaranteed valid until the next cache call, no callers cache the
 * pointer across yields. Today everything is pinned, so the contract
 * is trivially satisfied; Phase 4 makes pinning explicit.
 */

/* Progress callback type matches brfs.h's brfs_progress_callback_t.
 * We accept it as a typedef alias supplied by the caller; declare it
 * here for self-contained use of this header. */
#ifndef BRFS_PROGRESS_CALLBACK_T_DEFINED
#define BRFS_PROGRESS_CALLBACK_T_DEFINED
typedef void (*brfs_progress_callback_t)(const char *phase,
                                         unsigned int current,
                                         unsigned int total);
#endif

#define BRFS_CACHE_MAX_BLOCKS 65536  /* matches BRFS_MAX_BLOCKS */

typedef struct brfs_cache {
    brfs_storage_t *storage;

    /* Linear cache buffer, supplied by the caller at init time. */
    unsigned int *buf;
    unsigned int  buf_words;

    /* Storage byte addresses of the three on-disk regions. */
    unsigned int superblock_addr;
    unsigned int fat_addr;
    unsigned int data_addr;

    /* Layout, populated by brfs_cache_configure() after format/mount. */
    unsigned int total_blocks;
    unsigned int words_per_block;

    /* Per-FS-block dirty bitmap. */
    unsigned int dirty[(BRFS_CACHE_MAX_BLOCKS + 31) / 32];
} brfs_cache_t;

/* One-shot wiring: associate cache with backend + buffer. Does no I/O. */
void brfs_cache_init(brfs_cache_t *c,
                     brfs_storage_t *storage,
                     unsigned int *buf,
                     unsigned int buf_words);

/* Set on-disk region offsets (must be called before any I/O). */
void brfs_cache_set_layout(brfs_cache_t *c,
                           unsigned int superblock_addr,
                           unsigned int fat_addr,
                           unsigned int data_addr);

/* Tell the cache about the formatted/mounted FS geometry. */
void brfs_cache_configure(brfs_cache_t *c,
                          unsigned int total_blocks,
                          unsigned int words_per_block);

/* Pointer accessors into the cache buffer. */
unsigned int *brfs_cache_superblock(brfs_cache_t *c);
unsigned int *brfs_cache_fat(brfs_cache_t *c);
unsigned int *brfs_cache_data(brfs_cache_t *c, unsigned int block_idx);

/* Dirty tracking. */
void brfs_cache_mark_dirty(brfs_cache_t *c, unsigned int block_idx);
void brfs_cache_clear_dirty(brfs_cache_t *c);

/* Read just the 16-word superblock from storage into the cache. */
int brfs_cache_load_superblock(brfs_cache_t *c);

/* Erase + write the superblock sector back to storage. */
int brfs_cache_flush_superblock(brfs_cache_t *c);

/* Bulk-load FAT + data region from storage into the cache. */
int brfs_cache_load(brfs_cache_t *c, brfs_progress_callback_t progress);

/* Flush every dirty FS-block to storage, sector-granular. Clears dirty bits. */
int brfs_cache_flush(brfs_cache_t *c, brfs_progress_callback_t progress);

#endif /* FPGC_BRFS_CACHE_H */
