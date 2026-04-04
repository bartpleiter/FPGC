/*
 * Test: Exact brfs_sync two-loop pattern.
 * This test replicates the EXACT structure of brfs_sync:
 * - setup phase computing sectors/blocks from superblock
 * - FIRST nested loop (FAT sectors) with inner dirty check + conditional write
 * - SECOND nested loop (data sectors) with inner dirty check + conditional write  
 * - Final cleanup loop clearing dirty blocks
 *
 * Uses small values so the simulation completes within cycle limits.
 * The FLASH_WORDS_PER_SECTOR constant is set to 8 instead of 1024.
 */

void interrupt(void) {}

#define WORDS_PER_SECTOR 8

/* Mock superblock */
struct superblock {
    unsigned int total_blocks;
    unsigned int words_per_block;
};

struct superblock mock_sb;
int dirty_blocks[32];
int fat_write_log[8];
int data_write_log[8];
int fat_write_count;
int data_write_count;
int last_progress_step;
int last_progress_total;

struct superblock *get_superblock(void)
{
    return &mock_sb;
}

int is_block_dirty(unsigned int block_idx)
{
    if (block_idx < 32)
        return dirty_blocks[block_idx];
    return 0;
}

void write_fat_sector(unsigned int sector_idx)
{
    if (fat_write_count < 8)
        fat_write_log[fat_write_count] = sector_idx;
    fat_write_count++;
}

void write_data_sector(unsigned int sector_idx)
{
    if (data_write_count < 8)
        data_write_log[data_write_count] = sector_idx;
    data_write_count++;
}

void report_progress(char *label, unsigned int step, unsigned int total)
{
    (void)label;
    last_progress_step = step;
    last_progress_total = total;
}

int do_sync(void)
{
    struct superblock *sb;
    unsigned int blocks_per_sector;
    unsigned int sector;
    unsigned int block;
    unsigned int i;
    unsigned int fat_sectors;
    unsigned int data_sectors;
    int sector_dirty;
    unsigned int progress_total;
    unsigned int progress_step;

    sb = get_superblock();

    blocks_per_sector = WORDS_PER_SECTOR / sb->words_per_block;
    if (blocks_per_sector == 0)
    {
        blocks_per_sector = 1;
    }

    fat_sectors = (sb->total_blocks + WORDS_PER_SECTOR - 1) / WORDS_PER_SECTOR;
    data_sectors = (sb->total_blocks * sb->words_per_block + WORDS_PER_SECTOR - 1) / WORDS_PER_SECTOR;
    progress_total = fat_sectors + data_sectors;
    progress_step = 0;

    /* FIRST LOOP: FAT sectors (same as brfs_sync) */
    for (sector = 0; sector < fat_sectors; sector++)
    {
        sector_dirty = 0;

        for (i = 0; i < WORDS_PER_SECTOR && !sector_dirty; i++)
        {
            block = sector * WORDS_PER_SECTOR + i;
            if (block < sb->total_blocks && is_block_dirty(block))
            {
                sector_dirty = 1;
            }
        }

        if (sector_dirty)
        {
            write_fat_sector(sector);
        }

        progress_step++;
        report_progress("sync-fat", progress_step, progress_total);
    }

    /* SECOND LOOP: data sectors (same as brfs_sync) */
    for (sector = 0; sector < data_sectors; sector++)
    {
        sector_dirty = 0;

        for (i = 0; i < blocks_per_sector && !sector_dirty; i++)
        {
            block = sector * blocks_per_sector + i;
            if (block < sb->total_blocks && is_block_dirty(block))
            {
                sector_dirty = 1;
            }
        }

        if (sector_dirty)
        {
            write_data_sector(sector);
        }

        progress_step++;
        report_progress("sync-data", progress_step, progress_total);
    }

    /* CLEANUP: clear dirty blocks */
    for (i = 0; i < 32; i++)
    {
        dirty_blocks[i] = 0;
    }

    return 0;
}

int main(void)
{
    int result;

    /* Setup: 16 blocks, 2 words per block */
    mock_sb.total_blocks = 16;
    mock_sb.words_per_block = 2;

    /* blocks_per_sector = 8/2 = 4 */
    /* fat_sectors = (16+7)/8 = 2 */
    /* data_sectors = (16*2+7)/8 = 4 */
    /* progress_total = 2 + 4 = 6 */

    fat_write_count = 0;
    data_write_count = 0;

    /* Set some dirty blocks */
    dirty_blocks[0] = 1;   /* FAT sector 0, data sector 0 */
    dirty_blocks[5] = 1;   /* FAT sector 0, data sector 1 */
    dirty_blocks[10] = 1;  /* FAT sector 1, data sector 2 */
    dirty_blocks[15] = 1;  /* FAT sector 1, data sector 3 */

    do_sync();

    /* FAT: 2 sectors of 8 blocks each:
     *   sector 0 (blocks 0-7): dirty at 0,5 → write
     *   sector 1 (blocks 8-15): dirty at 10,15 → write
     * fat_write_count = 2
     */
    
    /* Data: 4 sectors of 4 blocks each:
     *   sector 0 (blocks 0-3): dirty at 0 → write
     *   sector 1 (blocks 4-7): dirty at 5 → write  
     *   sector 2 (blocks 8-11): dirty at 10 → write
     *   sector 3 (blocks 12-15): dirty at 15 → write
     * data_write_count = 4
     */

    /* Verify progress */
    /* last_progress_step = 6 (2 fat + 4 data) */

    /* Encode: fat_write_count * 32 + data_write_count * 4 + (last_progress_step == 6 ? 1 : 0) */
    result = fat_write_count * 32 + data_write_count * 4 + (last_progress_step == 6 ? 1 : 0);
    /* 2*32 + 4*4 + 1 = 64 + 16 + 1 = 81 = 0x51 */
    
    return result; // expected=0x51
}
