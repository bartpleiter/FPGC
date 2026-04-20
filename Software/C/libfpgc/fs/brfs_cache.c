#include "brfs_cache.h"
#include "brfs.h"   /* for BRFS_OK / BRFS_ERR_* and BRFS_FLASH_* layout consts */
#include <string.h>

/* ---- Temporary BRFS-mount debug tracing (UART) ---------------------- */
/* Define BRFS_DEBUG_TRACE=1 in build to enable UART trace prints during
 * brfs_cache_load_superblock and brfs_cache_load. Used to localize the
 * BDOS "stuck at 0% mount" hang. */
#ifndef BRFS_DEBUG_TRACE
#define BRFS_DEBUG_TRACE 1
#endif

#if BRFS_DEBUG_TRACE
#include "uart.h"
#define BRFS_TRACE(s)        uart_puts(s)
#define BRFS_TRACE_HEX(x)    uart_puthex((unsigned int)(x), 1)
#define BRFS_TRACE_INT(x)    uart_putint((int)(x))
#else
#define BRFS_TRACE(s)        ((void)0)
#define BRFS_TRACE_HEX(x)    ((void)0)
#define BRFS_TRACE_INT(x)    ((void)0)
#endif

/*
 * Phase 2 implementation: the cache is one linear buffer that mirrors
 * the entire on-disk image. Pinned, no eviction. Phase 4 swaps the
 * buffer for an LRU pool of fixed-size slots without changing this
 * header's API.
 */

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

void brfs_cache_configure(brfs_cache_t *c,
                          unsigned int total_blocks,
                          unsigned int words_per_block)
{
    c->total_blocks    = total_blocks;
    c->words_per_block = words_per_block;
}

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
    return c->buf + BRFS_SUPERBLOCK_SIZE + c->total_blocks +
           (block_idx * c->words_per_block);
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

static int cache_is_dirty(brfs_cache_t *c, unsigned int block_idx)
{
    return (c->dirty[block_idx >> 5] >> (block_idx & 31)) & 1u;
}

int brfs_cache_load_superblock(brfs_cache_t *c)
{
    int rc;
    BRFS_TRACE("[brfs] load_superblock: addr=");
    BRFS_TRACE_HEX(c->superblock_addr);
    BRFS_TRACE(" words=");
    BRFS_TRACE_INT(BRFS_SUPERBLOCK_SIZE);
    BRFS_TRACE("\n");
    rc = c->storage->read_words(c->storage, c->superblock_addr,
                                c->buf, BRFS_SUPERBLOCK_SIZE);
    BRFS_TRACE("[brfs] load_superblock: rc=");
    BRFS_TRACE_INT(rc);
    BRFS_TRACE("\n");
    return rc;
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
    unsigned int *data;
    unsigned int  words_remaining;
    unsigned int  words_this_sector;
    unsigned int  fat_sectors;
    unsigned int  data_sectors;
    unsigned int  sector;
    unsigned int  progress_total;
    unsigned int  progress_step;
    unsigned int  data_words;

    fat = brfs_cache_fat(c);
    data = brfs_cache_data(c, 0);

    fat_sectors  = (c->total_blocks + BRFS_FLASH_WORDS_PER_SECTOR - 1) /
                   BRFS_FLASH_WORDS_PER_SECTOR;
    data_words   = c->total_blocks * c->words_per_block;
    data_sectors = (data_words + BRFS_FLASH_WORDS_PER_SECTOR - 1) /
                   BRFS_FLASH_WORDS_PER_SECTOR;
    progress_total = fat_sectors + data_sectors;
    progress_step  = 0;

    /* FAT */
    BRFS_TRACE("[brfs] cache_load: fat_sectors=");
    BRFS_TRACE_INT(fat_sectors);
    BRFS_TRACE(" data_sectors=");
    BRFS_TRACE_INT(data_sectors);
    BRFS_TRACE(" data_words=");
    BRFS_TRACE_INT(data_words);
    BRFS_TRACE(" fat@");
    BRFS_TRACE_HEX(c->fat_addr);
    BRFS_TRACE(" data@");
    BRFS_TRACE_HEX(c->data_addr);
    BRFS_TRACE(" buf@");
    BRFS_TRACE_HEX((unsigned int)fat);
    BRFS_TRACE("\n");
    words_remaining = c->total_blocks;
    for (sector = 0; sector < fat_sectors; sector++) {
        words_this_sector = BRFS_FLASH_WORDS_PER_SECTOR;
        if (words_this_sector > words_remaining)
            words_this_sector = words_remaining;

        BRFS_TRACE("[brfs] FAT sec ");
        BRFS_TRACE_INT(sector);
        BRFS_TRACE(" addr=");
        BRFS_TRACE_HEX(c->fat_addr + (sector * BRFS_FLASH_SECTOR_SIZE));
        BRFS_TRACE(" dst=");
        BRFS_TRACE_HEX((unsigned int)(fat + (sector * BRFS_FLASH_WORDS_PER_SECTOR)));
        BRFS_TRACE(" w=");
        BRFS_TRACE_INT(words_this_sector);
        BRFS_TRACE(" ...");
        c->storage->read_words(c->storage,
            c->fat_addr + (sector * BRFS_FLASH_SECTOR_SIZE),
            fat + (sector * BRFS_FLASH_WORDS_PER_SECTOR),
            words_this_sector);
        BRFS_TRACE(" ok\n");

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

        BRFS_TRACE("[brfs] DAT sec ");
        BRFS_TRACE_INT(sector);
        BRFS_TRACE(" addr=");
        BRFS_TRACE_HEX(c->data_addr + (sector * BRFS_FLASH_SECTOR_SIZE));
        BRFS_TRACE(" dst=");
        BRFS_TRACE_HEX((unsigned int)(data + (sector * BRFS_FLASH_WORDS_PER_SECTOR)));
        BRFS_TRACE(" w=");
        BRFS_TRACE_INT(words_this_sector);
        BRFS_TRACE(" ...");
        c->storage->read_words(c->storage,
            c->data_addr + (sector * BRFS_FLASH_SECTOR_SIZE),
            data + (sector * BRFS_FLASH_WORDS_PER_SECTOR),
            words_this_sector);
        BRFS_TRACE(" ok\n");

        words_remaining -= words_this_sector;
        progress_step++;
        if (progress) progress("mount", progress_step, progress_total);
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

    data       = brfs_cache_data(c, 0);
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
    unsigned int blocks_per_sector;
    unsigned int sector;
    unsigned int block;
    unsigned int i;
    unsigned int fat_sectors;
    unsigned int data_sectors;
    int          sector_dirty;
    unsigned int progress_total;
    unsigned int progress_step;

    blocks_per_sector = BRFS_FLASH_WORDS_PER_SECTOR / c->words_per_block;
    if (blocks_per_sector == 0) blocks_per_sector = 1;

    fat_sectors  = (c->total_blocks + BRFS_FLASH_WORDS_PER_SECTOR - 1) /
                   BRFS_FLASH_WORDS_PER_SECTOR;
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

    brfs_cache_clear_dirty(c);
    return BRFS_OK;
}
