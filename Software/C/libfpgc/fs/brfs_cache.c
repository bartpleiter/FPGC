#include "brfs_cache.h"
#include "brfs.h"   /* for BRFS_OK / BRFS_ERR_* and BRFS_FLASH_* layout consts */
#include <string.h>

/*
 * Two modes:
 *   Linear — entire on-disk image fits in cache, all blocks pinned.
 *   LRU    — only superblock + FAT are pinned; data blocks live in a
 *            fixed-size slot pool with load-on-miss / evict-on-full.
 *
 * brfs_cache_configure() selects the mode automatically based on
 * whether the full linear image fits in buf_words.
 */

/* ---------- helpers ---------- */

#define LRU_SLOT_WORDS 4  /* sizeof(brfs_lru_slot_t) / sizeof(unsigned int) */

static int cache_is_dirty(brfs_cache_t *c, unsigned int block_idx)
{
    return (c->dirty[block_idx >> 5] >> (block_idx & 31)) & 1u;
}

static void cache_clear_dirty_bit(brfs_cache_t *c, unsigned int block_idx)
{
    c->dirty[block_idx >> 5] &= ~(1u << (block_idx & 31));
}

void brfs_cache_init(brfs_cache_t *c,
                     brfs_storage_t *storage,
                     unsigned int *buf,
                     unsigned int buf_words)
{
    unsigned int i;

    c->storage          = storage;
    c->buf              = buf;
    c->buf_words        = buf_words;
    c->superblock_addr  = 0;
    c->fat_addr         = 0;
    c->data_addr        = 0;
    c->total_blocks     = 0;
    c->words_per_block  = 0;

    c->lru_enabled      = 0;
    c->num_slots        = 0;
    c->slot_of          = NULL;
    c->slots            = NULL;
    c->data_base        = NULL;
    c->lru_head         = BRFS_LRU_SLOT_NONE;
    c->lru_tail         = BRFS_LRU_SLOT_NONE;

    for (i = 0; i < sizeof(c->dirty) / sizeof(c->dirty[0]); i++)
        c->dirty[i] = 0;
}

void brfs_cache_set_layout(brfs_cache_t *c,
                           unsigned int superblock_addr,
                           unsigned int fat_addr,
                           unsigned int data_addr)
{
    c->superblock_addr = superblock_addr;
    c->fat_addr        = fat_addr;
    c->data_addr       = data_addr;
}

static void lru_setup(brfs_cache_t *c);

void brfs_cache_configure(brfs_cache_t *c,
                          unsigned int total_blocks,
                          unsigned int words_per_block)
{
    unsigned int linear_size;

    c->total_blocks    = total_blocks;
    c->words_per_block = words_per_block;

    linear_size = BRFS_SUPERBLOCK_SIZE + total_blocks +
                  (total_blocks * words_per_block);

    if (linear_size <= c->buf_words)
    {
        c->lru_enabled = 0;
    }
    else
    {
        lru_setup(c);
    }
}

/* ---------- LRU list operations ---------- */

static void lru_remove(brfs_cache_t *c, unsigned int slot)
{
    unsigned int p = c->slots[slot].lru_prev;
    unsigned int n = c->slots[slot].lru_next;

    if (p != BRFS_LRU_SLOT_NONE)
        c->slots[p].lru_next = n;
    else
        c->lru_head = n;

    if (n != BRFS_LRU_SLOT_NONE)
        c->slots[n].lru_prev = p;
    else
        c->lru_tail = p;

    c->slots[slot].lru_prev = BRFS_LRU_SLOT_NONE;
    c->slots[slot].lru_next = BRFS_LRU_SLOT_NONE;
}

static void lru_push_front(brfs_cache_t *c, unsigned int slot)
{
    c->slots[slot].lru_prev = BRFS_LRU_SLOT_NONE;
    c->slots[slot].lru_next = c->lru_head;

    if (c->lru_head != BRFS_LRU_SLOT_NONE)
        c->slots[c->lru_head].lru_prev = slot;
    else
        c->lru_tail = slot;

    c->lru_head = slot;
}

static void lru_touch(brfs_cache_t *c, unsigned int slot)
{
    if (c->lru_head == slot)
        return;
    lru_remove(c, slot);
    lru_push_front(c, slot);
}

/* Flush a single slot's data block back to storage. */
static void lru_flush_slot(brfs_cache_t *c, unsigned int slot)
{
    unsigned int blk = c->slots[slot].block_idx;
    unsigned int addr = c->data_addr + blk * c->words_per_block * 4u;
    unsigned int *src = c->data_base + slot * c->words_per_block;

    c->storage->erase_sector(c->storage, addr);
    c->storage->write_words(c->storage, addr, src, c->words_per_block);
}

/* Evict the LRU-tail slot (skipping pinned ones). Returns slot index
 * or BRFS_LRU_SLOT_NONE if all slots are pinned. */
static unsigned int lru_evict(brfs_cache_t *c)
{
    unsigned int slot = c->lru_tail;

    while (slot != BRFS_LRU_SLOT_NONE && c->slots[slot].pin_count > 0)
        slot = c->slots[slot].lru_prev;

    if (slot == BRFS_LRU_SLOT_NONE)
        return BRFS_LRU_SLOT_NONE;

    /* Flush if dirty. */
    if (c->slots[slot].block_idx != BRFS_LRU_BLOCK_NONE &&
        cache_is_dirty(c, c->slots[slot].block_idx))
    {
        lru_flush_slot(c, slot);
        cache_clear_dirty_bit(c, c->slots[slot].block_idx);
    }

    /* Tear down old mapping. */
    if (c->slots[slot].block_idx != BRFS_LRU_BLOCK_NONE)
        c->slot_of[c->slots[slot].block_idx] = BRFS_LRU_SLOT_NONE;

    c->slots[slot].block_idx = BRFS_LRU_BLOCK_NONE;
    lru_remove(c, slot);
    return slot;
}

/* Initialise the LRU data structures inside the cache buffer.
 *
 * Buffer layout in LRU mode:
 *   [superblock 16w]
 *   [FAT total_blocks w]
 *   [slot_of total_blocks w]
 *   [slot metadata num_slots × 4w]
 *   [slot data num_slots × words_per_block w]
 */
static void lru_setup(brfs_cache_t *c)
{
    unsigned int overhead;
    unsigned int i;

    c->lru_enabled = 1;

    /* slot_of[] sits right after the FAT. */
    c->slot_of = c->buf + BRFS_SUPERBLOCK_SIZE + c->total_blocks;

    /* Compute how many data slots fit in the remaining space. */
    overhead = BRFS_SUPERBLOCK_SIZE + c->total_blocks + c->total_blocks;
    c->num_slots = (c->buf_words - overhead) / (c->words_per_block + LRU_SLOT_WORDS);

    /* Slot metadata follows slot_of[]. */
    c->slots = (brfs_lru_slot_t *)(c->buf + BRFS_SUPERBLOCK_SIZE +
                                   2u * c->total_blocks);

    /* Slot data follows slot metadata. */
    c->data_base = c->buf + BRFS_SUPERBLOCK_SIZE +
                   2u * c->total_blocks +
                   c->num_slots * LRU_SLOT_WORDS;

    /* Initialise slot_of[] — no block is cached yet. */
    for (i = 0; i < c->total_blocks; i++)
        c->slot_of[i] = BRFS_LRU_SLOT_NONE;

    /* Initialise all slots as free and link them into the LRU list. */
    for (i = 0; i < c->num_slots; i++)
    {
        c->slots[i].block_idx = BRFS_LRU_BLOCK_NONE;
        c->slots[i].pin_count = 0;
        c->slots[i].lru_prev  = (i > 0) ? i - 1 : BRFS_LRU_SLOT_NONE;
        c->slots[i].lru_next  = (i + 1 < c->num_slots) ? i + 1 : BRFS_LRU_SLOT_NONE;
    }
    c->lru_head = 0;
    c->lru_tail = c->num_slots - 1;
}

/* ---------- Pointer accessors ---------- */

unsigned int *brfs_cache_superblock(brfs_cache_t *c)
{
    return c->buf;
}

unsigned int *brfs_cache_fat(brfs_cache_t *c)
{
    return c->buf + BRFS_SUPERBLOCK_SIZE;
}

unsigned int *brfs_cache_data(brfs_cache_t *c, unsigned int block_idx)
{
    unsigned int slot;
    unsigned int addr;

    if (!c->lru_enabled)
    {
        /* Linear mode — direct offset into the buffer. */
        return c->buf + BRFS_SUPERBLOCK_SIZE + c->total_blocks +
               (block_idx * c->words_per_block);
    }

    /* --- LRU mode --- */

    slot = c->slot_of[block_idx];
    if (slot != BRFS_LRU_SLOT_NONE)
    {
        /* Cache hit — promote to MRU. */
        lru_touch(c, slot);
        return c->data_base + slot * c->words_per_block;
    }

    /* Cache miss — allocate a slot (may evict). */
    slot = lru_evict(c);
    if (slot == BRFS_LRU_SLOT_NONE)
        return NULL;   /* all pinned — shouldn't happen */

    /* Load block from storage. */
    addr = c->data_addr + block_idx * c->words_per_block * 4u;
    c->storage->read_words(c->storage, addr,
                           c->data_base + slot * c->words_per_block,
                           c->words_per_block);

    /* Establish mapping. */
    c->slots[slot].block_idx = block_idx;
    c->slots[slot].pin_count = 0;
    c->slot_of[block_idx] = slot;
    lru_push_front(c, slot);

    return c->data_base + slot * c->words_per_block;
}

void brfs_cache_mark_dirty(brfs_cache_t *c, unsigned int block_idx)
{
    c->dirty[block_idx >> 5] |= (1u << (block_idx & 31));
}

void brfs_cache_clear_dirty(brfs_cache_t *c)
{
    unsigned int i;
    for (i = 0; i < sizeof(c->dirty) / sizeof(c->dirty[0]); i++)
        c->dirty[i] = 0;
}

int brfs_cache_load_superblock(brfs_cache_t *c)
{
    return c->storage->read_words(c->storage, c->superblock_addr,
                                  c->buf, BRFS_SUPERBLOCK_SIZE);
}

int brfs_cache_flush_superblock(brfs_cache_t *c)
{
    int rc;
    rc = c->storage->erase_sector(c->storage, c->superblock_addr);
    if (rc != 0) return rc;
    return c->storage->write_words(c->storage, c->superblock_addr,
                                   c->buf, BRFS_SUPERBLOCK_SIZE);
}

int brfs_cache_load(brfs_cache_t *c, brfs_progress_callback_t progress)
{
    unsigned int *fat;
    unsigned int  words_remaining;
    unsigned int  words_this_sector;
    unsigned int  fat_sectors;
    unsigned int  sector;
    unsigned int  progress_total;
    unsigned int  progress_step;

    fat = brfs_cache_fat(c);

    fat_sectors  = (c->total_blocks + BRFS_FLASH_WORDS_PER_SECTOR - 1) /
                   BRFS_FLASH_WORDS_PER_SECTOR;

    if (c->lru_enabled)
    {
        /* LRU mode — load FAT only; data blocks loaded on demand. */
        progress_total = fat_sectors;
        progress_step  = 0;

        words_remaining = c->total_blocks;
        for (sector = 0; sector < fat_sectors; sector++) {
            words_this_sector = BRFS_FLASH_WORDS_PER_SECTOR;
            if (words_this_sector > words_remaining)
                words_this_sector = words_remaining;
            c->storage->read_words(c->storage,
                c->fat_addr + (sector * BRFS_FLASH_SECTOR_SIZE),
                fat + (sector * BRFS_FLASH_WORDS_PER_SECTOR),
                words_this_sector);
            words_remaining -= words_this_sector;
            progress_step++;
            if (progress) progress("mount", progress_step, progress_total);
        }

        brfs_cache_clear_dirty(c);
        return BRFS_OK;
    }

    /* Linear mode — load FAT + all data. */
    {
        unsigned int *data;
        unsigned int  data_words;
        unsigned int  data_sectors;

        data = brfs_cache_data(c, 0);
        data_words   = c->total_blocks * c->words_per_block;
        data_sectors = (data_words + BRFS_FLASH_WORDS_PER_SECTOR - 1) /
                       BRFS_FLASH_WORDS_PER_SECTOR;
        progress_total = fat_sectors + data_sectors;
        progress_step  = 0;

        /* FAT */
        words_remaining = c->total_blocks;
        for (sector = 0; sector < fat_sectors; sector++) {
            words_this_sector = BRFS_FLASH_WORDS_PER_SECTOR;
            if (words_this_sector > words_remaining)
                words_this_sector = words_remaining;
            c->storage->read_words(c->storage,
                c->fat_addr + (sector * BRFS_FLASH_SECTOR_SIZE),
                fat + (sector * BRFS_FLASH_WORDS_PER_SECTOR),
                words_this_sector);
            words_remaining -= words_this_sector;
            progress_step++;
            if (progress) progress("mount", progress_step, progress_total);
        }

        /* Data */
        words_remaining = data_words;
        for (sector = 0; sector < data_sectors; sector++) {
            words_this_sector = BRFS_FLASH_WORDS_PER_SECTOR;
            if (words_this_sector > words_remaining)
                words_this_sector = words_remaining;
            c->storage->read_words(c->storage,
                c->data_addr + (sector * BRFS_FLASH_SECTOR_SIZE),
                data + (sector * BRFS_FLASH_WORDS_PER_SECTOR),
                words_this_sector);
            words_remaining -= words_this_sector;
            progress_step++;
            if (progress) progress("mount", progress_step, progress_total);
        }
    }

    brfs_cache_clear_dirty(c);
    return BRFS_OK;
}

static void flush_fat_sector(brfs_cache_t *c, unsigned int sector_idx)
{
    unsigned int flash_addr;
    unsigned int *fat;
    unsigned int  fat_offset;
    unsigned int  page;

    fat        = brfs_cache_fat(c);
    flash_addr = c->fat_addr + (sector_idx * BRFS_FLASH_SECTOR_SIZE);
    fat_offset = sector_idx * BRFS_FLASH_WORDS_PER_SECTOR;

    c->storage->erase_sector(c->storage, flash_addr);

    for (page = 0; page < 16; page++) {
        c->storage->write_words(c->storage,
            flash_addr + (page * BRFS_FLASH_PAGE_SIZE),
            fat + fat_offset + (page * BRFS_FLASH_WORDS_PER_PAGE),
            BRFS_FLASH_WORDS_PER_PAGE);
    }
}

static void flush_data_sector(brfs_cache_t *c, unsigned int sector_idx)
{
    unsigned int  flash_addr;
    unsigned int *data;
    unsigned int  page;

    /* Linear mode only — contiguous data region starting at block 0. */
    data       = c->buf + BRFS_SUPERBLOCK_SIZE + c->total_blocks;
    flash_addr = c->data_addr + (sector_idx * BRFS_FLASH_SECTOR_SIZE);

    c->storage->erase_sector(c->storage, flash_addr);

    for (page = 0; page < 16; page++) {
        c->storage->write_words(c->storage,
            flash_addr + (page * BRFS_FLASH_PAGE_SIZE),
            data + (sector_idx * BRFS_FLASH_WORDS_PER_SECTOR) +
                   (page * BRFS_FLASH_WORDS_PER_PAGE),
            BRFS_FLASH_WORDS_PER_PAGE);
    }
}

int brfs_cache_flush(brfs_cache_t *c, brfs_progress_callback_t progress)
{
    unsigned int sector;
    unsigned int block;
    unsigned int i;
    unsigned int fat_sectors;
    int          sector_dirty;

    fat_sectors  = (c->total_blocks + BRFS_FLASH_WORDS_PER_SECTOR - 1) /
                   BRFS_FLASH_WORDS_PER_SECTOR;

    if (c->lru_enabled)
    {
        unsigned int slot;
        unsigned int progress_total;
        unsigned int progress_step;

        /* In LRU mode: flush FAT (always, conservatively), then dirty slots. */
        progress_total = fat_sectors + c->num_slots;
        progress_step  = 0;

        /* FAT — use same dirty-block heuristic as linear mode. */
        for (sector = 0; sector < fat_sectors; sector++) {
            sector_dirty = 0;
            for (i = 0; i < BRFS_FLASH_WORDS_PER_SECTOR && !sector_dirty; i++) {
                block = sector * BRFS_FLASH_WORDS_PER_SECTOR + i;
                if (block < c->total_blocks && cache_is_dirty(c, block))
                    sector_dirty = 1;
            }
            if (sector_dirty)
                flush_fat_sector(c, sector);

            progress_step++;
            if (progress) progress("sync-fat", progress_step, progress_total);
        }

        /* Data — iterate slots, flush dirty ones. */
        for (slot = 0; slot < c->num_slots; slot++) {
            if (c->slots[slot].block_idx != BRFS_LRU_BLOCK_NONE &&
                cache_is_dirty(c, c->slots[slot].block_idx))
            {
                lru_flush_slot(c, slot);
                cache_clear_dirty_bit(c, c->slots[slot].block_idx);
            }
            progress_step++;
            if (progress) progress("sync-data", progress_step, progress_total);
        }

        brfs_cache_clear_dirty(c);
        return BRFS_OK;
    }

    /* Linear mode — sector-granular flush. */
    {
        unsigned int blocks_per_sector;
        unsigned int data_sectors;
        unsigned int progress_total;
        unsigned int progress_step;

        blocks_per_sector = BRFS_FLASH_WORDS_PER_SECTOR / c->words_per_block;
        if (blocks_per_sector == 0) blocks_per_sector = 1;

        data_sectors = (c->total_blocks * c->words_per_block +
                        BRFS_FLASH_WORDS_PER_SECTOR - 1) /
                       BRFS_FLASH_WORDS_PER_SECTOR;
        progress_total = fat_sectors + data_sectors;
        progress_step  = 0;

        for (sector = 0; sector < fat_sectors; sector++) {
            sector_dirty = 0;
            for (i = 0; i < BRFS_FLASH_WORDS_PER_SECTOR && !sector_dirty; i++) {
                block = sector * BRFS_FLASH_WORDS_PER_SECTOR + i;
                if (block < c->total_blocks && cache_is_dirty(c, block))
                    sector_dirty = 1;
            }
            if (sector_dirty)
                flush_fat_sector(c, sector);

            progress_step++;
            if (progress) progress("sync-fat", progress_step, progress_total);
        }

        for (sector = 0; sector < data_sectors; sector++) {
            sector_dirty = 0;
            for (i = 0; i < blocks_per_sector && !sector_dirty; i++) {
                block = sector * blocks_per_sector + i;
                if (block < c->total_blocks && cache_is_dirty(c, block))
                    sector_dirty = 1;
            }
            if (sector_dirty)
                flush_data_sector(c, sector);

            progress_step++;
            if (progress) progress("sync-data", progress_step, progress_total);
        }
    }

    brfs_cache_clear_dirty(c);
    return BRFS_OK;
}

/* ---------- Pin / Unpin ---------- */

void brfs_cache_pin(brfs_cache_t *c, unsigned int block_idx)
{
    unsigned int slot;
    if (!c->lru_enabled) return;
    slot = c->slot_of[block_idx];
    if (slot == BRFS_LRU_SLOT_NONE) return;
    if (c->slots[slot].pin_count == 0)
        lru_remove(c, slot);   /* remove from eviction pool */
    c->slots[slot].pin_count++;
}

void brfs_cache_unpin(brfs_cache_t *c, unsigned int block_idx)
{
    unsigned int slot;
    if (!c->lru_enabled) return;
    slot = c->slot_of[block_idx];
    if (slot == BRFS_LRU_SLOT_NONE) return;
    if (c->slots[slot].pin_count > 0)
    {
        c->slots[slot].pin_count--;
        if (c->slots[slot].pin_count == 0)
            lru_push_front(c, slot);   /* back into eviction pool at MRU */
    }
}
