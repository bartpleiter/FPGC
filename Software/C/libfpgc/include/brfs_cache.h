#ifndef FPGC_BRFS_CACHE_H
#define FPGC_BRFS_CACHE_H

#include "brfs_storage.h"

/*
 * BRFS in-RAM block cache.
 *
 * Two operating modes:
 *
 * 1. **Linear (pinned)** — the entire on-disk image fits in the cache
 *    buffer: [superblock | FAT | data].  Every block is always resident.
 *    Used for SPI flash.
 *
 * 2. **LRU** — the data region is larger than the cache buffer. The
 *    superblock and FAT are always pinned; data blocks live in a
 *    fixed-size slot pool managed by a doubly-linked LRU list.  Cache
 *    misses load on demand; eviction flushes dirty slots.  Used for
 *    the SD card.
 *
 * The mode is selected automatically by brfs_cache_configure(): if the
 * full linear image fits in buf_words the cache uses linear mode,
 * otherwise it switches to LRU.
 *
 * brfs_cache_data(blk) returns a pointer that is guaranteed valid until
 * the next brfs_cache_data() call.  Callers must never hold two data
 * pointers across a brfs_cache_data() call.
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

#define BRFS_LRU_SLOT_NONE   0xFFFFFFFFu
#define BRFS_LRU_BLOCK_NONE  0xFFFFFFFFu

/* Per-slot metadata for LRU mode.  Stored as 4 consecutive words in
 * the cache buffer (sizeof == 4 * sizeof(unsigned int) == 16 bytes). */
typedef struct brfs_lru_slot {
    unsigned int block_idx;     /* FS data block held, or BRFS_LRU_BLOCK_NONE */
    unsigned int pin_count;     /* >0 means pinned, cannot evict */
    unsigned int lru_prev;      /* prev slot in LRU chain, or BRFS_LRU_SLOT_NONE */
    unsigned int lru_next;      /* next slot in LRU chain, or BRFS_LRU_SLOT_NONE */
} brfs_lru_slot_t;

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

    /* Per-FS-block dirty bitmap (used in both modes). */
    unsigned int dirty[(BRFS_CACHE_MAX_BLOCKS + 31) / 32];

    /* --- LRU mode fields (only meaningful when lru_enabled != 0) --- */
    int           lru_enabled;  /* 0 = linear, 1 = LRU */
    unsigned int  num_slots;    /* number of data-block slots */
    unsigned int *slot_of;      /* [total_blocks] block→slot, in buf */
    brfs_lru_slot_t *slots;     /* [num_slots] slot metadata, in buf */
    unsigned int *data_base;    /* start of slot data area, in buf */
    unsigned int  lru_head;     /* MRU end of LRU list */
    unsigned int  lru_tail;     /* LRU end (eviction candidate) */
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

/* Pin / unpin a data block (LRU mode only; no-op in linear mode).
 * A pinned block cannot be evicted.  Every pin must be balanced by an unpin. */
void brfs_cache_pin(brfs_cache_t *c, unsigned int block_idx);
void brfs_cache_unpin(brfs_cache_t *c, unsigned int block_idx);

#endif /* FPGC_BRFS_CACHE_H */
