/*
 * Test: brfs_sync-like pattern — the exact pattern that crashes BDOS.
 * Many locals, nested loops, function calls inside inner loop,
 * conditional logic based on call results.
 *
 * This mimics brfs_sync: outer loop over sectors, inner loop
 * over blocks within a sector, calling a check function for each
 * block, accumulating a flag, then conditionally calling a write
 * function.
 */

void interrupt(void) {}

int dirty_blocks[32];
int write_log[8];
int write_count;
int progress_count;

void init_dirty(void)
{
    int i;
    for (i = 0; i < 32; i++)
        dirty_blocks[i] = (i % 5 == 0) ? 1 : 0;
    write_count = 0;
    progress_count = 0;
}

int is_block_dirty(int block_idx)
{
    if (block_idx < 32)
        return dirty_blocks[block_idx];
    return 0;
}

void write_sector(int sector_idx)
{
    if (write_count < 8)
        write_log[write_count] = sector_idx;
    write_count++;
}

void report_progress(int current, int total)
{
    (void)total;
    progress_count = current;
}

int main(void)
{
    int total_blocks;
    int blocks_per_sector;
    int total_sectors;
    int sector, block, block_idx;
    int needs_write;
    int progress;
    int result;

    total_blocks = 32;
    blocks_per_sector = 8;
    total_sectors = 4;
    progress = 0;
    result = 0;

    init_dirty();

    /* Outer loop: iterate over sectors */
    for (sector = 0; sector < total_sectors; sector++)
    {
        needs_write = 0;

        /* Inner loop: check if any block in this sector is dirty */
        for (block = 0; block < blocks_per_sector && !needs_write; block++)
        {
            block_idx = sector * blocks_per_sector + block;
            if (block_idx < total_blocks)
            {
                if (is_block_dirty(block_idx))
                {
                    needs_write = 1;
                }
            }
        }

        if (needs_write)
        {
            write_sector(sector);
        }

        progress = progress + 1;
        report_progress(progress, total_sectors);
    }

    /* dirty blocks at: 0, 5, 10, 15, 20, 25, 30
     * sector 0 (blocks 0-7): dirty at 0,5 → write
     * sector 1 (blocks 8-15): dirty at 10,15 → write
     * sector 2 (blocks 16-23): dirty at 20 → write
     * sector 3 (blocks 24-31): dirty at 25,30 → write
     * write_count = 4, progress_count = 4
     */
    result = write_count * 16 + progress_count;
    /* result = 4*16 + 4 = 68 = 0x44 */
    return result; // expected=0x44
}
