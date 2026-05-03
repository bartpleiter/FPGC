#include "brfs_storage_sdcard.h"
#include "sd.h"

/*
 * BRFS v2 storage backend for SD card.
 *
 * BRFS gives us byte-addressed offsets and word-granular counts;
 * SD speaks 512-byte blocks. We translate by:
 *   - Splitting the request into a head partial block (if `addr`
 *     isn't 512-aligned), a body of full blocks, and a tail partial
 *     block (if the trailing byte count isn't 512-aligned).
 *   - Doing read-modify-write through a 512 B scratch buffer for the
 *     partial blocks; the body uses sd_read_blocks / sd_write_blocks
 *     directly so the DMA fast path on SPI5 carries the bulk traffic.
 *
 * The scratch buffer is module-static; BRFS calls into the storage
 * layer one operation at a time so re-entrancy is not a concern in
 * the current single-foreground OS.
 *
 * erase_sector is a no-op. SD cards present a clean overwrite-anywhere
 * interface; the on-card controller does its own wear-levelling and
 * erase scheduling.
 */

#define SDB_BYTES SD_BLOCK_SIZE   /* 512 */

/*
 * Scratch buffer for partial-block read-modify-write.
 *
 * Intentionally NOT 32-byte aligned: SD_DMA_OK(sdb_scratch) will
 * fail, forcing sd_read/write_block to use the CPU byte-at-a-time
 * SPI path. The DMA SPI burst path has an unresolved reliability
 * issue (see sd-brfs-implementation.md "DMA Investigation").
 * The CPU path is slower but correct.
 */
static unsigned char sdb_scratch[SDB_BYTES];

static int
sd_rmw_partial_read(unsigned int lba, unsigned int off,
                    void *dst, unsigned int n_bytes)
{
    sd_result_t r = sd_read_block(lba, sdb_scratch);
    if (r != SD_OK)
        return -1;
    {
        unsigned int i;
        unsigned char *d = (unsigned char *)dst;
        for (i = 0; i < n_bytes; i++)
            d[i] = sdb_scratch[off + i];
    }
    return 0;
}

static int
sd_rmw_partial_write(unsigned int lba, unsigned int off,
                     const void *src, unsigned int n_bytes)
{
    sd_result_t r = sd_read_block(lba, sdb_scratch);
    if (r != SD_OK)
        return -1;
    {
        unsigned int i;
        const unsigned char *s = (const unsigned char *)src;
        for (i = 0; i < n_bytes; i++)
            sdb_scratch[off + i] = s[i];
    }
    r = sd_write_block(lba, sdb_scratch);
    return (r == SD_OK) ? 0 : -1;
}

static int
sd_read_words_op(brfs_storage_t *self,
                 unsigned int addr,
                 unsigned int *dst,
                 unsigned int n_words)
{
    unsigned char *d = (unsigned char *)dst;
    unsigned int n_bytes = n_words * 4u;
    unsigned int lba = addr / SDB_BYTES;
    unsigned int off = addr % SDB_BYTES;
    sd_result_t r;

    (void)self;
    if (n_bytes == 0u)
        return 0;

    /* Head partial. */
    if (off != 0u) {
        unsigned int chunk = SDB_BYTES - off;
        if (chunk > n_bytes)
            chunk = n_bytes;
        if (sd_rmw_partial_read(lba, off, d, chunk) < 0)
            return -1;
        d       += chunk;
        n_bytes -= chunk;
        lba     += 1u;
    }

    /* Body: aligned full blocks.
     * DMA SPI2MEM to the BRFS cache at high SDRAM addresses fails for
     * a still-unknown reason; bounce through the manually-aligned
     * sdb_scratch buffer (in kernel BSS) which SD_DMA_OK passes for. */
    while (n_bytes >= SDB_BYTES) {
        r = sd_read_block(lba, sdb_scratch);
        if (r != SD_OK)
            return -1;
        {
            unsigned int ci;
            for (ci = 0; ci < SDB_BYTES; ci++)
                d[ci] = sdb_scratch[ci];
        }
        d       += SDB_BYTES;
        n_bytes -= SDB_BYTES;
        lba     += 1u;
    }

    /* Tail partial. */
    if (n_bytes != 0u) {
        if (sd_rmw_partial_read(lba, 0u, d, n_bytes) < 0)
            return -1;
    }

    return 0;
}

static int
sd_write_words_op(brfs_storage_t *self,
                  unsigned int addr,
                  const unsigned int *src,
                  unsigned int n_words)
{
    const unsigned char *s = (const unsigned char *)src;
    unsigned int n_bytes = n_words * 4u;
    unsigned int lba = addr / SDB_BYTES;
    unsigned int off = addr % SDB_BYTES;
    sd_result_t r;

    (void)self;
    if (n_bytes == 0u)
        return 0;

    /* Head partial. */
    if (off != 0u) {
        unsigned int chunk = SDB_BYTES - off;
        if (chunk > n_bytes)
            chunk = n_bytes;
        if (sd_rmw_partial_write(lba, off, s, chunk) < 0)
            return -1;
        s       += chunk;
        n_bytes -= chunk;
        lba     += 1u;
    }

    /* Body: aligned full blocks.
     * Bounce through sdb_scratch (manually aligned, DMA-eligible). */
    while (n_bytes >= SDB_BYTES) {
        {
            unsigned int ci;
            for (ci = 0; ci < SDB_BYTES; ci++)
                sdb_scratch[ci] = s[ci];
        }
        r = sd_write_block(lba, sdb_scratch);
        if (r != SD_OK)
            return -1;
        s       += SDB_BYTES;
        n_bytes -= SDB_BYTES;
        lba     += 1u;
    }

    /* Tail partial. */
    if (n_bytes != 0u) {
        if (sd_rmw_partial_write(lba, 0u, s, n_bytes) < 0)
            return -1;
    }

    return 0;
}

static int
sd_erase_sector_op(brfs_storage_t *self, unsigned int addr)
{
    /* SD has no host-visible erase. The card's controller schedules
     * its own erases internally; an overwrite is enough for BRFS. */
    (void)self;
    (void)addr;
    return 0;
}

void
brfs_storage_sdcard_init(brfs_sdcard_storage_t *out)
{
    out->base.read_words   = sd_read_words_op;
    out->base.write_words  = sd_write_words_op;
    out->base.erase_sector = sd_erase_sector_op;
}
