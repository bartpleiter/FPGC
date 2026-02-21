// Bart's RAM File System (BRFS) Implementation
// See brfs.h for architecture documentation and API details.

#include "libs/kernel/fs/brfs.h"
#include "libs/kernel/io/spi_flash.h"
#include "libs/common/string.h"

// ---- Global State ----
struct brfs_state brfs;
static brfs_progress_callback_t brfs_progress_callback = NULL;

// brfs report progress
static void brfs_report_progress(const char* phase, unsigned int current, unsigned int total)
{
    if (brfs_progress_callback != NULL)
    {
        brfs_progress_callback(phase, current, total);
    }
}

// ---- Internal Helper Functions - Forward Declarations ----
static unsigned int* brfs_get_superblock();
static unsigned int* brfs_get_fat();
static unsigned int* brfs_get_data_block(unsigned int block_idx);
static int brfs_find_free_block();
static int brfs_find_free_dir_entry(unsigned int* dir_block);
static int brfs_get_dir_fat_idx(const char* dir_path);
static int brfs_find_in_directory(unsigned int dir_fat_idx, const char* name, 
                                   struct brfs_dir_entry** entry_out);
static void brfs_mark_block_dirty(unsigned int block_idx);
static int brfs_get_fat_idx_at_offset(unsigned int start_fat_idx, unsigned int offset);
static void brfs_init_directory_block(unsigned int* block_addr, unsigned int dir_fat_idx, 
                                       unsigned int parent_fat_idx);
static void brfs_create_dir_entry(struct brfs_dir_entry* entry, const char* filename,
                                   unsigned int fat_idx, unsigned int filesize, 
                                   unsigned int flags);

// ---- String compression/decompression ----
// Filenames are stored with 4 characters packed per word.
// chars[0] uses bits 31-24, chars[1] uses bits 23-16, etc.

void brfs_compress_string(unsigned int* dest, const char* src)
{
    unsigned int word;
    unsigned int char_idx;
    unsigned int word_idx;
    unsigned int c;
    
    word_idx = 0;
    word = 0;
    char_idx = 0;
    
    while (1)
    {
        c = (unsigned int)src[char_idx] & 0xFF;
        
        // Pack character into current position
        word |= c << (24 - (char_idx & 3) * 8);
        
        // End of string?
        if (c == 0)
        {
            dest[word_idx] = word;
            // Zero out remaining words if needed
            word_idx++;
            while (word_idx < 4)
            {
                dest[word_idx] = 0;
                word_idx++;
            }
            break;
        }
        
        char_idx++;
        
        // Completed a word?
        if ((char_idx & 3) == 0)
        {
            dest[word_idx] = word;
            word_idx++;
            word = 0;
            
            // Reached maximum filename length?
            if (word_idx >= 4)
            {
                break;
            }
        }
    }
}

// brfs decompress string
void brfs_decompress_string(char* dest, const unsigned int* src, unsigned int src_words)
{
    unsigned int word_idx;
    unsigned int char_idx;
    unsigned int word;
    unsigned int c;
    
    char_idx = 0;
    
    for (word_idx = 0; word_idx < src_words; word_idx++)
    {
        word = src[word_idx];
        
        c = (word >> 24) & 0xFF;
        dest[char_idx++] = c;
        if (c == 0) return;
        
        c = (word >> 16) & 0xFF;
        dest[char_idx++] = c;
        if (c == 0) return;
        
        c = (word >> 8) & 0xFF;
        dest[char_idx++] = c;
        if (c == 0) return;
        
        c = word & 0xFF;
        dest[char_idx++] = c;
        if (c == 0) return;
    }
    
    dest[char_idx] = '\0';
}

// ---- Path Parsing ----
int brfs_parse_path(const char* path, char* dir_path, char* filename, 
                    unsigned int dir_path_size)
{
    int len;
    int i;
    int last_slash;
    
    if (path == NULL || dir_path == NULL || filename == NULL)
    {
        return BRFS_ERR_INVALID_PARAM;
    }
    
    len = strlen(path);
    
    if (len == 0 || len > BRFS_MAX_PATH_LENGTH)
    {
        return BRFS_ERR_PATH_TOO_LONG;
    }
    
    // Find last slash
    last_slash = -1;
    for (i = len - 1; i >= 0; i--)
    {
        if (path[i] == '/')
        {
            last_slash = i;
            break;
        }
    }
    
    if (last_slash < 0)
    {
        // No slash found - treat as file in root
        dir_path[0] = '/';
        dir_path[1] = '\0';
        strcpy(filename, path);
    }
    else if (last_slash == 0)
    {
        // File in root directory
        dir_path[0] = '/';
        dir_path[1] = '\0';
        strcpy(filename, path + 1);
    }
    else
    {
        // Copy directory path
        if ((unsigned int)last_slash >= dir_path_size)
        {
            return BRFS_ERR_PATH_TOO_LONG;
        }
        
        for (i = 0; i < last_slash; i++)
        {
            dir_path[i] = path[i];
        }
        dir_path[last_slash] = '\0';
        
        // Copy filename
        strcpy(filename, path + last_slash + 1);
    }
    
    // Validate filename length
    if (strlen(filename) == 0 || strlen(filename) > BRFS_MAX_FILENAME_LENGTH)
    {
        return BRFS_ERR_NAME_TOO_LONG;
    }
    
    return BRFS_OK;
}

// ---- Internal Cache Access Functions ----
static unsigned int* brfs_get_superblock()
{
    return brfs.cache;
}

// brfs get fat
static unsigned int* brfs_get_fat()
{
    return brfs.cache + BRFS_SUPERBLOCK_SIZE;
}

// brfs get data block
static unsigned int* brfs_get_data_block(unsigned int block_idx)
{
    struct brfs_superblock* sb;
    
    sb = (struct brfs_superblock*)brfs_get_superblock();
    return brfs.cache + BRFS_SUPERBLOCK_SIZE + sb->total_blocks + 
           (block_idx * sb->words_per_block);
}

// ---- Block Allocation Functions ----
static int brfs_find_free_block()
{
    struct brfs_superblock* sb;
    unsigned int* fat;
    unsigned int i;
    
    sb = (struct brfs_superblock*)brfs_get_superblock();
    fat = brfs_get_fat();
    
    for (i = 0; i < sb->total_blocks; i++)
    {
        if (fat[i] == BRFS_FAT_FREE)
        {
            return (int)i;
        }
    }
    
    return BRFS_ERR_NO_SPACE;
}

// brfs find free dir entry
static int brfs_find_free_dir_entry(unsigned int* dir_block)
{
    struct brfs_superblock* sb;
    unsigned int max_entries;
    unsigned int i;
    struct brfs_dir_entry* entry;
    
    sb = (struct brfs_superblock*)brfs_get_superblock();
    max_entries = sb->words_per_block / BRFS_DIR_ENTRY_SIZE;
    
    for (i = 0; i < max_entries; i++)
    {
        entry = (struct brfs_dir_entry*)(dir_block + (i * BRFS_DIR_ENTRY_SIZE));
        if (entry->filename[0] == 0)
        {
            return (int)i;
        }
    }
    
    return BRFS_ERR_NO_ENTRY;
}

// brfs mark block dirty
static void brfs_mark_block_dirty(unsigned int block_idx)
{
    brfs.dirty_blocks[block_idx >> 5] |= (1u << (block_idx & 31));
}

// brfs is block dirty
static int brfs_is_block_dirty(unsigned int block_idx)
{
    return (brfs.dirty_blocks[block_idx >> 5] >> (block_idx & 31)) & 1;
}

// ---- FAT Chain Navigation ----
static int brfs_get_fat_idx_at_offset(unsigned int start_fat_idx, unsigned int offset)
{
    struct brfs_superblock* sb;
    unsigned int* fat;
    unsigned int current_idx;
    unsigned int blocks_to_skip;
    
    sb = (struct brfs_superblock*)brfs_get_superblock();
    fat = brfs_get_fat();
    
    current_idx = start_fat_idx;
    blocks_to_skip = offset / sb->words_per_block;
    
    while (blocks_to_skip > 0)
    {
        current_idx = fat[current_idx];
        if (current_idx == BRFS_FAT_EOF)
        {
            return BRFS_ERR_SEEK_ERROR;
        }
        blocks_to_skip--;
    }
    
    return (int)current_idx;
}

// ---- Directory Navigation and Lookup ----
// Get the FAT index of a directory given its path
// Returns the FAT index or a negative error code
static int brfs_get_dir_fat_idx(const char* dir_path)
{
    struct brfs_superblock* sb;
    unsigned int current_fat_idx;
    char path_copy[BRFS_MAX_PATH_LENGTH + 1];
    char token[BRFS_MAX_FILENAME_LENGTH + 1];
    int path_idx;
    int token_idx;
    int len;
    struct brfs_dir_entry* found_entry;
    int result;
    
    if (dir_path == NULL)
    {
        return BRFS_ERR_INVALID_PARAM;
    }
    
    len = strlen(dir_path);
    if (len > BRFS_MAX_PATH_LENGTH)
    {
        return BRFS_ERR_PATH_TOO_LONG;
    }
    
    // Root directory
    if (len == 0 || (len == 1 && dir_path[0] == '/'))
    {
        return 0;
    }
    
    sb = (struct brfs_superblock*)brfs_get_superblock();
    current_fat_idx = 0; // Start at root
    
    // Copy path for tokenization
    strcpy(path_copy, dir_path);
    
    // Skip leading slash
    path_idx = 0;
    if (path_copy[0] == '/')
    {
        path_idx = 1;
    }
    
    // Parse path components
    while (path_copy[path_idx] != '\0')
    {
        // Extract next path component
        token_idx = 0;
        while (path_copy[path_idx] != '\0' && path_copy[path_idx] != '/')
        {
            if (token_idx >= BRFS_MAX_FILENAME_LENGTH)
            {
                return BRFS_ERR_NAME_TOO_LONG;
            }
            token[token_idx++] = path_copy[path_idx++];
        }
        token[token_idx] = '\0';
        
        // Skip trailing slash
        if (path_copy[path_idx] == '/')
        {
            path_idx++;
        }
        
        // Skip empty components (multiple slashes)
        if (token_idx == 0)
        {
            continue;
        }
        
        // Look up this component in current directory
        result = brfs_find_in_directory(current_fat_idx, token, &found_entry);
        if (result != BRFS_OK)
        {
            return result;
        }
        
        // Must be a directory
        if ((found_entry->flags & BRFS_FLAG_DIRECTORY) == 0)
        {
            return BRFS_ERR_NOT_DIRECTORY;
        }
        
        current_fat_idx = found_entry->fat_idx;
    }
    
    return (int)current_fat_idx;
}

// Find an entry in a directory by name
// If found, sets entry_out to point to the entry in the cache
static int brfs_find_in_directory(unsigned int dir_fat_idx, const char* name,
                                   struct brfs_dir_entry** entry_out)
{
    struct brfs_superblock* sb;
    unsigned int* dir_block;
    unsigned int max_entries;
    unsigned int i;
    struct brfs_dir_entry* entry;
    char entry_name[BRFS_MAX_FILENAME_LENGTH + 1];
    
    sb = (struct brfs_superblock*)brfs_get_superblock();
    dir_block = brfs_get_data_block(dir_fat_idx);
    max_entries = sb->words_per_block / BRFS_DIR_ENTRY_SIZE;
    
    for (i = 0; i < max_entries; i++)
    {
        entry = (struct brfs_dir_entry*)(dir_block + (i * BRFS_DIR_ENTRY_SIZE));
        
        if (entry->filename[0] != 0)
        {
            brfs_decompress_string(entry_name, entry->filename, 4);
            
            if (strcmp(entry_name, name) == 0)
            {
                if (entry_out != NULL)
                {
                    *entry_out = entry;
                }
                return BRFS_OK;
            }
        }
    }
    
    return BRFS_ERR_NOT_FOUND;
}

// ---- Directory Entry Creation ----
static void brfs_create_dir_entry(struct brfs_dir_entry* entry, const char* filename,
                                   unsigned int fat_idx, unsigned int filesize,
                                   unsigned int flags)
{
    memset(entry, 0, sizeof(struct brfs_dir_entry));
    brfs_compress_string(entry->filename, filename);
    entry->fat_idx = fat_idx;
    entry->filesize = filesize;
    entry->flags = flags;
    entry->modify_date = 0; // TODO: Add RTC support
}

static void brfs_init_directory_block(unsigned int* block_addr, unsigned int dir_fat_idx,
                                       unsigned int parent_fat_idx)
{
    struct brfs_superblock* sb;
    struct brfs_dir_entry entry;
    unsigned int max_entries;
    
    sb = (struct brfs_superblock*)brfs_get_superblock();
    max_entries = sb->words_per_block / BRFS_DIR_ENTRY_SIZE;
    
    // Zero the entire block
    memset(block_addr, 0, sb->words_per_block);
    
    // Create "." entry
    brfs_create_dir_entry(&entry, ".", dir_fat_idx, 
                          max_entries * BRFS_DIR_ENTRY_SIZE, BRFS_FLAG_DIRECTORY);
    memcpy(block_addr, &entry, sizeof(entry));
    
    // Create ".." entry
    brfs_create_dir_entry(&entry, "..", parent_fat_idx,
                          max_entries * BRFS_DIR_ENTRY_SIZE, BRFS_FLAG_DIRECTORY);
    memcpy(block_addr + BRFS_DIR_ENTRY_SIZE, &entry, sizeof(entry));
}

// ---- Initialization Functions ----
int brfs_init(unsigned int flash_id)
{
    unsigned int i;
    
    // Initialize state
    brfs.cache = (unsigned int*)BRFS_CACHE_ADDR;
    brfs.cache_size = BRFS_MAX_CACHE_SIZE;
    brfs.initialized = 0;
    brfs.flash_id = flash_id;
    brfs.flash_superblock_addr = BRFS_FLASH_SUPERBLOCK_ADDR;
    brfs.flash_fat_addr = BRFS_FLASH_FAT_ADDR;
    brfs.flash_data_addr = BRFS_FLASH_DATA_ADDR;
    
    // Clear open files table
    for (i = 0; i < BRFS_MAX_OPEN_FILES; i++)
    {
        brfs.open_files[i].fat_idx = 0;
        brfs.open_files[i].cursor = 0;
        brfs.open_files[i].dir_entry = NULL;
    }
    
    // Clear dirty blocks bitmap
    for (i = 0; i < sizeof(brfs.dirty_blocks) / sizeof(brfs.dirty_blocks[0]); i++)
    {
        brfs.dirty_blocks[i] = 0;
    }
    
    return BRFS_OK;
}

// brfs set progress callback
void brfs_set_progress_callback(brfs_progress_callback_t callback)
{
    brfs_progress_callback = callback;
}

int brfs_format(unsigned int total_blocks, unsigned int words_per_block,
                const char* label, int full_format)
{
    struct brfs_superblock* sb;
    unsigned int* fat;
    unsigned int* root_block;
    unsigned int i;
    unsigned int data_size;
    unsigned int data_words;
    unsigned int data_sectors;
    unsigned int sector;
    unsigned int sector_offset;
    unsigned int words_in_sector;
    
    // Validate parameters
    if (total_blocks == 0 || total_blocks > BRFS_MAX_BLOCKS)
    {
        return BRFS_ERR_INVALID_PARAM;
    }
    
    if (words_per_block == 0 || words_per_block > 2048)
    {
        return BRFS_ERR_INVALID_PARAM;
    }
    
    // Block count should be multiple of 64 for efficient dirty tracking
    if ((total_blocks & 63) != 0)
    {
        return BRFS_ERR_INVALID_PARAM;
    }
    
    // Words per block should be multiple of 64 for flash page alignment
    if ((words_per_block & 63) != 0)
    {
        return BRFS_ERR_INVALID_PARAM;
    }
    
    // Check cache size
    data_size = BRFS_SUPERBLOCK_SIZE + total_blocks + (total_blocks * words_per_block);
    if (data_size > brfs.cache_size)
    {
        return BRFS_ERR_NO_SPACE;
    }
    
    // Initialize superblock
    sb = (struct brfs_superblock*)brfs_get_superblock();
    memset(sb, 0, sizeof(struct brfs_superblock));
    
    sb->total_blocks = total_blocks;
    sb->words_per_block = words_per_block;
    sb->brfs_version = BRFS_VERSION;
    
    // Copy label (max 10 characters)
    if (label != NULL)
    {
        for (i = 0; i < 10 && label[i] != '\0'; i++)
        {
            sb->label[i] = (unsigned int)label[i];
        }
    }
    
    // Initialize FAT - all blocks free
    fat = brfs_get_fat();
    memset(fat, 0, total_blocks);
    
    // Full format: zero all data blocks
    if (full_format)
    {
        root_block = brfs_get_data_block(0);
        data_words = total_blocks * words_per_block;
        data_sectors = (data_words + BRFS_FLASH_WORDS_PER_SECTOR - 1) / BRFS_FLASH_WORDS_PER_SECTOR;

        for (sector = 0; sector < data_sectors; sector++)
        {
            sector_offset = sector * BRFS_FLASH_WORDS_PER_SECTOR;
            words_in_sector = BRFS_FLASH_WORDS_PER_SECTOR;

            if ((sector_offset + words_in_sector) > data_words)
            {
                words_in_sector = data_words - sector_offset;
            }

            memset(root_block + sector_offset, 0, words_in_sector);
            brfs_report_progress("format-zero", sector + 1, data_sectors);
        }
    }
    
    // Initialize root directory (block 0)
    root_block = brfs_get_data_block(0);
    brfs_init_directory_block(root_block, 0, 0);
    
    // Mark root block as used (end of chain)
    fat[0] = BRFS_FAT_EOF;
    
    // Mark all blocks as dirty
    for (i = 0; i < total_blocks; i++)
    {
        brfs_mark_block_dirty(i);
    }
    
    // Write superblock to flash immediately
    spi_flash_erase_sector(brfs.flash_id, brfs.flash_superblock_addr);
    spi_flash_write_words(brfs.flash_id, brfs.flash_superblock_addr, 
                         (unsigned int*)sb, BRFS_SUPERBLOCK_SIZE);
    
    brfs.initialized = 1;
    
    return BRFS_OK;
}

// ---- Superblock Validation ----
static int brfs_validate_superblock(struct brfs_superblock* sb)
{
    if (sb->brfs_version != BRFS_VERSION)
    {
        return BRFS_ERR_INVALID_SUPERBLOCK;
    }
    
    if (sb->total_blocks == 0 || sb->total_blocks > BRFS_MAX_BLOCKS)
    {
        return BRFS_ERR_INVALID_SUPERBLOCK;
    }
    
    if ((sb->total_blocks & 63) != 0)
    {
        return BRFS_ERR_INVALID_SUPERBLOCK;
    }
    
    if (sb->words_per_block == 0 || sb->words_per_block > 2048)
    {
        return BRFS_ERR_INVALID_SUPERBLOCK;
    }
    
    return BRFS_OK;
}

// ---- Mount/Unmount Functions ----
int brfs_mount()
{
    struct brfs_superblock* sb;
    unsigned int* fat;
    unsigned int* data;
    unsigned int data_size;
    unsigned int i;
    int result;
    unsigned int words_remaining;
    unsigned int words_this_sector;
    unsigned int fat_sectors;
    unsigned int data_sectors;
    unsigned int sector;
    unsigned int progress_total;
    unsigned int progress_step;
    
    // Read superblock from flash
    sb = (struct brfs_superblock*)brfs_get_superblock();
    spi_flash_read_words(brfs.flash_id, brfs.flash_superblock_addr,
                        (unsigned int*)sb, BRFS_SUPERBLOCK_SIZE);
    
    // Validate superblock
    result = brfs_validate_superblock(sb);
    if (result != BRFS_OK)
    {
        return result;
    }
    
    // Check if filesystem fits in cache
    data_size = BRFS_SUPERBLOCK_SIZE + sb->total_blocks + 
                (sb->total_blocks * sb->words_per_block);
    if (data_size > brfs.cache_size)
    {
        return BRFS_ERR_NO_SPACE;
    }
    
    // Read FAT from flash (with progress)
    fat = brfs_get_fat();
    words_remaining = sb->total_blocks;
    fat_sectors = (words_remaining + BRFS_FLASH_WORDS_PER_SECTOR - 1) / BRFS_FLASH_WORDS_PER_SECTOR;
    data_sectors = (sb->total_blocks * sb->words_per_block + BRFS_FLASH_WORDS_PER_SECTOR - 1) / BRFS_FLASH_WORDS_PER_SECTOR;
    progress_total = fat_sectors + data_sectors;
    progress_step = 0;

    for (sector = 0; sector < fat_sectors; sector++)
    {
        words_this_sector = BRFS_FLASH_WORDS_PER_SECTOR;
        if (words_this_sector > words_remaining)
        {
            words_this_sector = words_remaining;
        }

        spi_flash_read_words(brfs.flash_id,
                            brfs.flash_fat_addr + (sector * BRFS_FLASH_SECTOR_SIZE),
                            fat + (sector * BRFS_FLASH_WORDS_PER_SECTOR),
                            words_this_sector);

        words_remaining -= words_this_sector;
        progress_step++;
        brfs_report_progress("mount", progress_step, progress_total);
    }
    
    // Read data blocks from flash (with progress)
    data = brfs_get_data_block(0);
    data_size = sb->total_blocks * sb->words_per_block;
    words_remaining = data_size;
    data_sectors = (words_remaining + BRFS_FLASH_WORDS_PER_SECTOR - 1) / BRFS_FLASH_WORDS_PER_SECTOR;

    for (sector = 0; sector < data_sectors; sector++)
    {
        words_this_sector = BRFS_FLASH_WORDS_PER_SECTOR;
        if (words_this_sector > words_remaining)
        {
            words_this_sector = words_remaining;
        }

        spi_flash_read_words(brfs.flash_id,
                            brfs.flash_data_addr + (sector * BRFS_FLASH_SECTOR_SIZE),
                            data + (sector * BRFS_FLASH_WORDS_PER_SECTOR),
                            words_this_sector);

        words_remaining -= words_this_sector;
        progress_step++;
        brfs_report_progress("mount", progress_step, progress_total);
    }
    
    // Clear dirty flags - everything is in sync with flash
    for (i = 0; i < sizeof(brfs.dirty_blocks) / sizeof(brfs.dirty_blocks[0]); i++)
    {
        brfs.dirty_blocks[i] = 0;
    }
    
    // Clear open files table
    for (i = 0; i < BRFS_MAX_OPEN_FILES; i++)
    {
        brfs.open_files[i].fat_idx = 0;
        brfs.open_files[i].cursor = 0;
        brfs.open_files[i].dir_entry = NULL;
    }
    
    brfs.initialized = 1;
    
    return BRFS_OK;
}

// brfs unmount
int brfs_unmount()
{
    int result;
    unsigned int i;
    
    if (!brfs.initialized)
    {
        return BRFS_ERR_NOT_INITIALIZED;
    }
    
    // Sync dirty data to flash
    result = brfs_sync();
    if (result != BRFS_OK)
    {
        return result;
    }
    
    // Close all open files
    for (i = 0; i < BRFS_MAX_OPEN_FILES; i++)
    {
        brfs.open_files[i].fat_idx = 0;
        brfs.open_files[i].cursor = 0;
        brfs.open_files[i].dir_entry = NULL;
    }
    
    brfs.initialized = 0;
    
    return BRFS_OK;
}

// ---- Flash Sync Functions ----
static void brfs_write_fat_sector(unsigned int sector_idx)
{
    unsigned int flash_addr;
    unsigned int* fat;
    unsigned int fat_offset;
    unsigned int page;
    
    fat = brfs_get_fat();
    flash_addr = brfs.flash_fat_addr + (sector_idx * BRFS_FLASH_SECTOR_SIZE);
    fat_offset = sector_idx * BRFS_FLASH_WORDS_PER_SECTOR;
    
    // Erase sector
    spi_flash_erase_sector(brfs.flash_id, flash_addr);
    
    // Write 16 pages of 64 words each
    for (page = 0; page < 16; page++)
    {
        spi_flash_write_words(brfs.flash_id, 
                            flash_addr + (page * BRFS_FLASH_PAGE_SIZE),
                            fat + fat_offset + (page * BRFS_FLASH_WORDS_PER_PAGE),
                            BRFS_FLASH_WORDS_PER_PAGE);
    }
}

// brfs write data sector
static void brfs_write_data_sector(unsigned int sector_idx)
{
    struct brfs_superblock* sb;
    unsigned int flash_addr;
    unsigned int* data;
    unsigned int page;
    
    sb = (struct brfs_superblock*)brfs_get_superblock();
    data = brfs_get_data_block(0);
    
    flash_addr = brfs.flash_data_addr + (sector_idx * BRFS_FLASH_SECTOR_SIZE);
    
    // Erase sector
    spi_flash_erase_sector(brfs.flash_id, flash_addr);
    
    // Write 16 pages of 64 words each
    for (page = 0; page < 16; page++)
    {
        spi_flash_write_words(brfs.flash_id,
                            flash_addr + (page * BRFS_FLASH_PAGE_SIZE),
                            data + (sector_idx * BRFS_FLASH_WORDS_PER_SECTOR) + 
                                   (page * BRFS_FLASH_WORDS_PER_PAGE),
                            BRFS_FLASH_WORDS_PER_PAGE);
    }
}

// brfs sync
int brfs_sync()
{
    struct brfs_superblock* sb;
    unsigned int blocks_per_sector;
    unsigned int sector;
    unsigned int block;
    unsigned int i;
    unsigned int fat_sectors;
    unsigned int data_sectors;
    int sector_dirty;
    unsigned int progress_total;
    unsigned int progress_step;
    
    if (!brfs.initialized)
    {
        return BRFS_ERR_NOT_INITIALIZED;
    }
    
    sb = (struct brfs_superblock*)brfs_get_superblock();
    
    // Calculate blocks per 4KB sector
    blocks_per_sector = BRFS_FLASH_WORDS_PER_SECTOR / sb->words_per_block;
    if (blocks_per_sector == 0)
    {
        blocks_per_sector = 1; // Block larger than sector
    }
    
    // Calculate number of sectors for FAT and data
    fat_sectors = (sb->total_blocks + BRFS_FLASH_WORDS_PER_SECTOR - 1) / 
                   BRFS_FLASH_WORDS_PER_SECTOR;
    data_sectors = (sb->total_blocks * sb->words_per_block + 
                    BRFS_FLASH_WORDS_PER_SECTOR - 1) / BRFS_FLASH_WORDS_PER_SECTOR;
    progress_total = fat_sectors + data_sectors;
    progress_step = 0;
    
    // Write dirty FAT sectors
    for (sector = 0; sector < fat_sectors; sector++)
    {
        sector_dirty = 0;
        
        // Check if any block in this FAT sector is dirty
        for (i = 0; i < BRFS_FLASH_WORDS_PER_SECTOR && !sector_dirty; i++)
        {
            block = sector * BRFS_FLASH_WORDS_PER_SECTOR + i;
            if (block < sb->total_blocks && brfs_is_block_dirty(block))
            {
                sector_dirty = 1;
            }
        }
        
        if (sector_dirty)
        {
            brfs_write_fat_sector(sector);
        }

        progress_step++;
        brfs_report_progress("sync-fat", progress_step, progress_total);
    }
    
    // Write dirty data sectors
    for (sector = 0; sector < data_sectors; sector++)
    {
        sector_dirty = 0;
        
        // Check if any block in this data sector is dirty
        for (i = 0; i < blocks_per_sector && !sector_dirty; i++)
        {
            block = sector * blocks_per_sector + i;
            if (block < sb->total_blocks && brfs_is_block_dirty(block))
            {
                sector_dirty = 1;
            }
        }
        
        if (sector_dirty)
        {
            brfs_write_data_sector(sector);
        }

        progress_step++;
        brfs_report_progress("sync-data", progress_step, progress_total);
    }
    
    // Clear dirty flags
    for (i = 0; i < sizeof(brfs.dirty_blocks) / sizeof(brfs.dirty_blocks[0]); i++)
    {
        brfs.dirty_blocks[i] = 0;
    }
    
    return BRFS_OK;
}

// ---- File Creation ----
int brfs_create_file(const char* path)
{
    char dir_path[BRFS_MAX_PATH_LENGTH + 1];
    char filename[BRFS_MAX_FILENAME_LENGTH + 1];
    int result;
    int dir_fat_idx;
    int free_block;
    int free_entry_idx;
    unsigned int* dir_block;
    unsigned int* fat;
    struct brfs_dir_entry* entry;
    struct brfs_dir_entry new_entry;
    struct brfs_superblock* sb;
    
    if (!brfs.initialized)
    {
        return BRFS_ERR_NOT_INITIALIZED;
    }
    
    // Parse path
    result = brfs_parse_path(path, dir_path, filename, sizeof(dir_path));
    if (result != BRFS_OK)
    {
        return result;
    }
    
    // Get parent directory
    dir_fat_idx = brfs_get_dir_fat_idx(dir_path);
    if (dir_fat_idx < 0)
    {
        return dir_fat_idx;
    }
    
    // Check if file already exists
    result = brfs_find_in_directory(dir_fat_idx, filename, &entry);
    if (result == BRFS_OK)
    {
        return BRFS_ERR_EXISTS;
    }
    
    // Find free block for file
    free_block = brfs_find_free_block();
    if (free_block < 0)
    {
        return free_block;
    }
    
    // Find free directory entry
    dir_block = brfs_get_data_block(dir_fat_idx);
    free_entry_idx = brfs_find_free_dir_entry(dir_block);
    if (free_entry_idx < 0)
    {
        return free_entry_idx;
    }
    
    // Create directory entry
    brfs_create_dir_entry(&new_entry, filename, free_block, 0, 0);
    
    // Write to directory
    entry = (struct brfs_dir_entry*)(dir_block + (free_entry_idx * BRFS_DIR_ENTRY_SIZE));
    memcpy(entry, &new_entry, sizeof(struct brfs_dir_entry));
    
    // Update FAT
    fat = brfs_get_fat();
    fat[free_block] = BRFS_FAT_EOF;
    
    // Initialize file block to zeros
    sb = (struct brfs_superblock*)brfs_get_superblock();
    memset(brfs_get_data_block(free_block), 0, sb->words_per_block);
    
    // Mark blocks dirty
    brfs_mark_block_dirty(dir_fat_idx);
    brfs_mark_block_dirty(free_block);
    
    return BRFS_OK;
}

// ---- Directory Creation ----
int brfs_create_dir(const char* path)
{
    char dir_path[BRFS_MAX_PATH_LENGTH + 1];
    char dirname[BRFS_MAX_FILENAME_LENGTH + 1];
    int result;
    int parent_fat_idx;
    int free_block;
    int free_entry_idx;
    unsigned int* parent_dir_block;
    unsigned int* new_dir_block;
    unsigned int* fat;
    struct brfs_dir_entry* entry;
    struct brfs_dir_entry new_entry;
    struct brfs_superblock* sb;
    unsigned int max_entries;
    
    if (!brfs.initialized)
    {
        return BRFS_ERR_NOT_INITIALIZED;
    }
    
    // Parse path
    result = brfs_parse_path(path, dir_path, dirname, sizeof(dir_path));
    if (result != BRFS_OK)
    {
        return result;
    }
    
    // Get parent directory
    parent_fat_idx = brfs_get_dir_fat_idx(dir_path);
    if (parent_fat_idx < 0)
    {
        return parent_fat_idx;
    }
    
    // Check if directory already exists
    result = brfs_find_in_directory(parent_fat_idx, dirname, &entry);
    if (result == BRFS_OK)
    {
        return BRFS_ERR_EXISTS;
    }
    
    // Find free block for directory
    free_block = brfs_find_free_block();
    if (free_block < 0)
    {
        return free_block;
    }
    
    // Find free directory entry in parent
    parent_dir_block = brfs_get_data_block(parent_fat_idx);
    free_entry_idx = brfs_find_free_dir_entry(parent_dir_block);
    if (free_entry_idx < 0)
    {
        return free_entry_idx;
    }
    
    // Get superblock for directory size calculation
    sb = (struct brfs_superblock*)brfs_get_superblock();
    max_entries = sb->words_per_block / BRFS_DIR_ENTRY_SIZE;
    
    // Create directory entry in parent
    brfs_create_dir_entry(&new_entry, dirname, free_block,
                          max_entries * BRFS_DIR_ENTRY_SIZE, BRFS_FLAG_DIRECTORY);
    
    // Write to parent directory
    entry = (struct brfs_dir_entry*)(parent_dir_block + (free_entry_idx * BRFS_DIR_ENTRY_SIZE));
    memcpy(entry, &new_entry, sizeof(struct brfs_dir_entry));
    
    // Initialize new directory block
    new_dir_block = brfs_get_data_block(free_block);
    brfs_init_directory_block(new_dir_block, free_block, parent_fat_idx);
    
    // Update FAT
    fat = brfs_get_fat();
    fat[free_block] = BRFS_FAT_EOF;
    
    // Mark blocks dirty
    brfs_mark_block_dirty(parent_fat_idx);
    brfs_mark_block_dirty(free_block);
    
    return BRFS_OK;
}

// ---- File Open/Close ----
int brfs_open(const char* path)
{
    char dir_path[BRFS_MAX_PATH_LENGTH + 1];
    char filename[BRFS_MAX_FILENAME_LENGTH + 1];
    int result;
    int dir_fat_idx;
    struct brfs_dir_entry* entry;
    int fd;
    unsigned int i;
    
    if (!brfs.initialized)
    {
        return BRFS_ERR_NOT_INITIALIZED;
    }
    
    // Parse path
    result = brfs_parse_path(path, dir_path, filename, sizeof(dir_path));
    if (result != BRFS_OK)
    {
        return result;
    }
    
    // Get directory containing the file
    dir_fat_idx = brfs_get_dir_fat_idx(dir_path);
    if (dir_fat_idx < 0)
    {
        return dir_fat_idx;
    }
    
    // Find file in directory
    result = brfs_find_in_directory(dir_fat_idx, filename, &entry);
    if (result != BRFS_OK)
    {
        return result;
    }
    
    // Cannot open directories as files
    if (entry->flags & BRFS_FLAG_DIRECTORY)
    {
        return BRFS_ERR_IS_DIRECTORY;
    }
    
    // Check if file is already open
    for (i = 0; i < BRFS_MAX_OPEN_FILES; i++)
    {
        if (brfs.open_files[i].fat_idx == entry->fat_idx && 
            brfs.open_files[i].dir_entry != NULL)
        {
            return BRFS_ERR_IS_OPEN;
        }
    }
    
    // Find free file descriptor
    fd = -1;
    for (i = 0; i < BRFS_MAX_OPEN_FILES; i++)
    {
        if (brfs.open_files[i].dir_entry == NULL)
        {
            fd = (int)i;
            break;
        }
    }
    
    if (fd < 0)
    {
        return BRFS_ERR_TOO_MANY_OPEN;
    }
    
    // Open file
    brfs.open_files[fd].fat_idx = entry->fat_idx;
    brfs.open_files[fd].cursor = 0;
    brfs.open_files[fd].dir_entry = entry;
    
    return fd;
}

// brfs close
int brfs_close(int fd)
{
    if (!brfs.initialized)
    {
        return BRFS_ERR_NOT_INITIALIZED;
    }
    
    if (fd < 0 || fd >= BRFS_MAX_OPEN_FILES)
    {
        return BRFS_ERR_INVALID_PARAM;
    }
    
    if (brfs.open_files[fd].dir_entry == NULL)
    {
        return BRFS_ERR_NOT_OPEN;
    }
    
    // Close file
    brfs.open_files[fd].fat_idx = 0;
    brfs.open_files[fd].cursor = 0;
    brfs.open_files[fd].dir_entry = NULL;
    
    return BRFS_OK;
}

// ---- File Read ----
int brfs_read(int fd, unsigned int* buffer, unsigned int length)
{
    struct brfs_superblock* sb;
    struct brfs_file* file;
    unsigned int* data_block;
    unsigned int* fat;
    unsigned int current_fat_idx;
    unsigned int cursor_in_block;
    unsigned int words_to_read;
    unsigned int words_until_end;
    unsigned int total_read;
    unsigned int remaining;
    
    if (!brfs.initialized)
    {
        return BRFS_ERR_NOT_INITIALIZED;
    }
    
    if (fd < 0 || fd >= BRFS_MAX_OPEN_FILES)
    {
        return BRFS_ERR_INVALID_PARAM;
    }
    
    file = &brfs.open_files[fd];
    
    if (file->dir_entry == NULL)
    {
        return BRFS_ERR_NOT_OPEN;
    }
    
    if (buffer == NULL)
    {
        return BRFS_ERR_INVALID_PARAM;
    }
    
    sb = (struct brfs_superblock*)brfs_get_superblock();
    fat = brfs_get_fat();
    
    // Truncate length to remaining file size
    if (file->cursor >= file->dir_entry->filesize)
    {
        return 0; // EOF
    }
    
    remaining = file->dir_entry->filesize - file->cursor;
    if (length > remaining)
    {
        length = remaining;
    }
    
    // Get FAT index at current cursor position
    current_fat_idx = brfs_get_fat_idx_at_offset(file->fat_idx, file->cursor);
    if ((int)current_fat_idx < 0)
    {
        return BRFS_ERR_READ_ERROR;
    }
    
    total_read = 0;
    
    while (length > 0)
    {
        cursor_in_block = file->cursor % sb->words_per_block;
        words_until_end = sb->words_per_block - cursor_in_block;
        words_to_read = (words_until_end < length) ? words_until_end : length;
        
        // Copy data from block
        data_block = brfs_get_data_block(current_fat_idx);
        memcpy(buffer, data_block + cursor_in_block, words_to_read);
        
        // Update state
        buffer += words_to_read;
        file->cursor += words_to_read;
        total_read += words_to_read;
        length -= words_to_read;
        
        // Move to next block if needed
        if (length > 0)
        {
            current_fat_idx = fat[current_fat_idx];
            if (current_fat_idx == BRFS_FAT_EOF)
            {
                break; // Unexpected EOF
            }
        }
    }
    
    return (int)total_read;
}

// ---- File Write ----
int brfs_write(int fd, const unsigned int* buffer, unsigned int length)
{
    struct brfs_superblock* sb;
    struct brfs_file* file;
    unsigned int* data_block;
    unsigned int* fat;
    unsigned int current_fat_idx;
    unsigned int cursor_in_block;
    unsigned int words_to_write;
    unsigned int words_until_end;
    unsigned int total_written;
    int next_block;
    int result;
    
    if (!brfs.initialized)
    {
        return BRFS_ERR_NOT_INITIALIZED;
    }
    
    if (fd < 0 || fd >= BRFS_MAX_OPEN_FILES)
    {
        return BRFS_ERR_INVALID_PARAM;
    }
    
    file = &brfs.open_files[fd];
    
    if (file->dir_entry == NULL)
    {
        return BRFS_ERR_NOT_OPEN;
    }
    
    if (buffer == NULL && length > 0)
    {
        return BRFS_ERR_INVALID_PARAM;
    }
    
    if (length == 0)
    {
        return 0;
    }
    
    sb = (struct brfs_superblock*)brfs_get_superblock();
    fat = brfs_get_fat();
    
    // Get FAT index at current cursor position
    result = brfs_get_fat_idx_at_offset(file->fat_idx, file->cursor);
    if (result < 0)
    {
        // Cursor is past current allocated blocks - need to allocate
        // For simplicity, only allow writing at or before EOF
        return BRFS_ERR_SEEK_ERROR;
    }
    current_fat_idx = (unsigned int)result;
    
    total_written = 0;
    
    while (length > 0)
    {
        cursor_in_block = file->cursor % sb->words_per_block;
        words_until_end = sb->words_per_block - cursor_in_block;
        words_to_write = (words_until_end < length) ? words_until_end : length;
        
        // Copy data to block
        data_block = brfs_get_data_block(current_fat_idx);
        memcpy(data_block + cursor_in_block, buffer, words_to_write);
        
        // Mark block as dirty
        brfs_mark_block_dirty(current_fat_idx);
        
        // Update state
        buffer += words_to_write;
        file->cursor += words_to_write;
        total_written += words_to_write;
        length -= words_to_write;
        
        // Need another block?
        if (length > 0)
        {
            if (fat[current_fat_idx] == BRFS_FAT_EOF)
            {
                // Allocate new block
                next_block = brfs_find_free_block();
                if (next_block < 0)
                {
                    // Update filesize with what we wrote
                    if (file->cursor > file->dir_entry->filesize)
                    {
                        file->dir_entry->filesize = file->cursor;
                    }
                    return (int)total_written;
                }
                
                // Link new block
                fat[current_fat_idx] = next_block;
                fat[next_block] = BRFS_FAT_EOF;
                
                // Initialize new block
                memset(brfs_get_data_block(next_block), 0, sb->words_per_block);
                brfs_mark_block_dirty(next_block);
                
                current_fat_idx = next_block;
            }
            else
            {
                current_fat_idx = fat[current_fat_idx];
            }
        }
    }
    
    // Update filesize if we extended the file
    if (file->cursor > file->dir_entry->filesize)
    {
        file->dir_entry->filesize = file->cursor;
    }
    
    return (int)total_written;
}

// ---- File Seek/Tell ----
int brfs_seek(int fd, unsigned int offset)
{
    struct brfs_file* file;
    
    if (!brfs.initialized)
    {
        return BRFS_ERR_NOT_INITIALIZED;
    }
    
    if (fd < 0 || fd >= BRFS_MAX_OPEN_FILES)
    {
        return BRFS_ERR_INVALID_PARAM;
    }
    
    file = &brfs.open_files[fd];
    
    if (file->dir_entry == NULL)
    {
        return BRFS_ERR_NOT_OPEN;
    }
    
    // Allow seeking beyond EOF (for subsequent writes)
    // But limit to filesize for safety
    if (offset > file->dir_entry->filesize)
    {
        offset = file->dir_entry->filesize;
    }
    
    file->cursor = offset;
    
    return (int)offset;
}

// brfs tell
int brfs_tell(int fd)
{
    struct brfs_file* file;
    
    if (!brfs.initialized)
    {
        return BRFS_ERR_NOT_INITIALIZED;
    }
    
    if (fd < 0 || fd >= BRFS_MAX_OPEN_FILES)
    {
        return BRFS_ERR_INVALID_PARAM;
    }
    
    file = &brfs.open_files[fd];
    
    if (file->dir_entry == NULL)
    {
        return BRFS_ERR_NOT_OPEN;
    }
    
    return (int)file->cursor;
}

// brfs file size
int brfs_file_size(int fd)
{
    struct brfs_file* file;
    
    if (!brfs.initialized)
    {
        return BRFS_ERR_NOT_INITIALIZED;
    }
    
    if (fd < 0 || fd >= BRFS_MAX_OPEN_FILES)
    {
        return BRFS_ERR_INVALID_PARAM;
    }
    
    file = &brfs.open_files[fd];
    
    if (file->dir_entry == NULL)
    {
        return BRFS_ERR_NOT_OPEN;
    }
    
    return (int)file->dir_entry->filesize;
}

// ---- Directory Reading ----
int brfs_read_dir(const char* path, struct brfs_dir_entry* buffer, unsigned int max_entries)
{
    struct brfs_superblock* sb;
    int dir_fat_idx;
    unsigned int* dir_block;
    unsigned int dir_max_entries;
    unsigned int count;
    unsigned int i;
    struct brfs_dir_entry* entry;
    
    if (!brfs.initialized)
    {
        return BRFS_ERR_NOT_INITIALIZED;
    }
    
    if (buffer == NULL)
    {
        return BRFS_ERR_INVALID_PARAM;
    }
    
    // Get directory FAT index
    dir_fat_idx = brfs_get_dir_fat_idx(path);
    if (dir_fat_idx < 0)
    {
        return dir_fat_idx;
    }
    
    sb = (struct brfs_superblock*)brfs_get_superblock();
    dir_block = brfs_get_data_block(dir_fat_idx);
    dir_max_entries = sb->words_per_block / BRFS_DIR_ENTRY_SIZE;
    
    count = 0;
    
    for (i = 0; i < dir_max_entries && count < max_entries; i++)
    {
        entry = (struct brfs_dir_entry*)(dir_block + (i * BRFS_DIR_ENTRY_SIZE));
        
        if (entry->filename[0] != 0)
        {
            memcpy(&buffer[count], entry, sizeof(struct brfs_dir_entry));
            count++;
        }
    }
    
    return (int)count;
}

// ---- Delete ----
int brfs_delete(const char* path)
{
    char dir_path[BRFS_MAX_PATH_LENGTH + 1];
    char filename[BRFS_MAX_FILENAME_LENGTH + 1];
    int result;
    int dir_fat_idx;
    struct brfs_dir_entry* entry;
    unsigned int* fat;
    unsigned int current_fat_idx;
    unsigned int next_fat_idx;
    unsigned int i;
    
    if (!brfs.initialized)
    {
        return BRFS_ERR_NOT_INITIALIZED;
    }
    
    // Parse path
    result = brfs_parse_path(path, dir_path, filename, sizeof(dir_path));
    if (result != BRFS_OK)
    {
        return result;
    }
    
    // Get directory containing the file
    dir_fat_idx = brfs_get_dir_fat_idx(dir_path);
    if (dir_fat_idx < 0)
    {
        return dir_fat_idx;
    }
    
    // Find file in directory
    result = brfs_find_in_directory(dir_fat_idx, filename, &entry);
    if (result != BRFS_OK)
    {
        return result;
    }
    
    // If it's a directory, check that it's empty
    if (entry->flags & BRFS_FLAG_DIRECTORY)
    {
        struct brfs_superblock* sb;
        unsigned int* target_dir_block;
        unsigned int max_entries;
        struct brfs_dir_entry* sub_entry;
        unsigned int non_empty_count;
        
        sb = (struct brfs_superblock*)brfs_get_superblock();
        target_dir_block = brfs_get_data_block(entry->fat_idx);
        max_entries = sb->words_per_block / BRFS_DIR_ENTRY_SIZE;
        
        non_empty_count = 0;
        for (i = 0; i < max_entries; i++)
        {
            sub_entry = (struct brfs_dir_entry*)(target_dir_block + (i * BRFS_DIR_ENTRY_SIZE));
            if (sub_entry->filename[0] != 0)
            {
                non_empty_count++;
            }
        }
        
        // Directory should only have . and ..
        if (non_empty_count > 2)
        {
            return BRFS_ERR_NOT_EMPTY;
        }
    }
    
    // Check if file is open
    for (i = 0; i < BRFS_MAX_OPEN_FILES; i++)
    {
        if (brfs.open_files[i].dir_entry == entry)
        {
            return BRFS_ERR_IS_OPEN;
        }
    }
    
    // Free all blocks in the chain
    fat = brfs_get_fat();
    current_fat_idx = entry->fat_idx;
    
    while (current_fat_idx != BRFS_FAT_EOF)
    {
        next_fat_idx = fat[current_fat_idx];
        fat[current_fat_idx] = BRFS_FAT_FREE;
        brfs_mark_block_dirty(current_fat_idx);
        current_fat_idx = next_fat_idx;
    }
    
    // Clear directory entry
    memset(entry, 0, sizeof(struct brfs_dir_entry));
    
    // Mark parent directory as dirty
    brfs_mark_block_dirty(dir_fat_idx);
    
    return BRFS_OK;
}

// ---- Stat Functions ----
int brfs_stat(const char* path, struct brfs_dir_entry* entry)
{
    char dir_path[BRFS_MAX_PATH_LENGTH + 1];
    char filename[BRFS_MAX_FILENAME_LENGTH + 1];
    int result;
    int dir_fat_idx;
    struct brfs_dir_entry* found_entry;
    int len;
    
    if (!brfs.initialized)
    {
        return BRFS_ERR_NOT_INITIALIZED;
    }
    
    if (entry == NULL)
    {
        return BRFS_ERR_INVALID_PARAM;
    }
    
    // Handle root directory specially
    len = strlen(path);
    if (len == 0 || (len == 1 && path[0] == '/'))
    {
        struct brfs_superblock* sb;
        sb = (struct brfs_superblock*)brfs_get_superblock();
        
        memset(entry, 0, sizeof(struct brfs_dir_entry));
        entry->flags = BRFS_FLAG_DIRECTORY;
        entry->fat_idx = 0;
        entry->filesize = sb->words_per_block;
        brfs_compress_string(entry->filename, "/");
        return BRFS_OK;
    }
    
    // Parse path
    result = brfs_parse_path(path, dir_path, filename, sizeof(dir_path));
    if (result != BRFS_OK)
    {
        return result;
    }
    
    // Get directory containing the file
    dir_fat_idx = brfs_get_dir_fat_idx(dir_path);
    if (dir_fat_idx < 0)
    {
        return dir_fat_idx;
    }
    
    // Find file in directory
    result = brfs_find_in_directory(dir_fat_idx, filename, &found_entry);
    if (result != BRFS_OK)
    {
        return result;
    }
    
    // Copy entry data
    memcpy(entry, found_entry, sizeof(struct brfs_dir_entry));
    
    return BRFS_OK;
}

// brfs exists
int brfs_exists(const char* path)
{
    struct brfs_dir_entry entry;
    
    return (brfs_stat(path, &entry) == BRFS_OK) ? 1 : 0;
}

// brfs is dir
int brfs_is_dir(const char* path)
{
    struct brfs_dir_entry entry;
    
    if (brfs_stat(path, &entry) != BRFS_OK)
    {
        return 0;
    }
    
    return (entry.flags & BRFS_FLAG_DIRECTORY) ? 1 : 0;
}

// ---- Filesystem Statistics ----
int brfs_statfs(unsigned int* total_blocks, unsigned int* free_blocks,
                unsigned int* block_size)
{
    struct brfs_superblock* sb;
    unsigned int* fat;
    unsigned int free_count;
    unsigned int i;
    
    if (!brfs.initialized)
    {
        return BRFS_ERR_NOT_INITIALIZED;
    }
    
    sb = (struct brfs_superblock*)brfs_get_superblock();
    fat = brfs_get_fat();
    
    // Count free blocks
    free_count = 0;
    for (i = 0; i < sb->total_blocks; i++)
    {
        if (fat[i] == BRFS_FAT_FREE)
        {
            free_count++;
        }
    }
    
    if (total_blocks != NULL)
    {
        *total_blocks = sb->total_blocks;
    }
    
    if (free_blocks != NULL)
    {
        *free_blocks = free_count;
    }
    
    if (block_size != NULL)
    {
        *block_size = sb->words_per_block;
    }
    
    return BRFS_OK;
}

// brfs get label
int brfs_get_label(char* label_buffer, unsigned int buffer_size)
{
    struct brfs_superblock* sb;
    unsigned int i;

    if (!brfs.initialized)
    {
        return BRFS_ERR_NOT_INITIALIZED;
    }

    if (label_buffer == NULL || buffer_size == 0)
    {
        return BRFS_ERR_INVALID_PARAM;
    }

    sb = (struct brfs_superblock*)brfs_get_superblock();

    i = 0;
    while (i < 10 && i < (buffer_size - 1))
    {
        char c;

        c = (char)(sb->label[i] & 0xFF);
        if (c == '\0')
        {
            break;
        }

        label_buffer[i] = c;
        i++;
    }

    label_buffer[i] = '\0';
    return BRFS_OK;
}

// ---- Error Strings ----
const char* brfs_strerror(int error_code)
{
    switch (error_code)
    {
        case BRFS_OK:                   return "Success";
        case BRFS_ERR_INVALID_PARAM:    return "Invalid parameter";
        case BRFS_ERR_NOT_FOUND:        return "Not found";
        case BRFS_ERR_EXISTS:           return "Already exists";
        case BRFS_ERR_NO_SPACE:         return "No space left";
        case BRFS_ERR_NO_ENTRY:         return "No free directory entry";
        case BRFS_ERR_NOT_EMPTY:        return "Directory not empty";
        case BRFS_ERR_IS_OPEN:          return "File is open";
        case BRFS_ERR_NOT_OPEN:         return "File is not open";
        case BRFS_ERR_TOO_MANY_OPEN:    return "Too many open files";
        case BRFS_ERR_IS_DIRECTORY:     return "Is a directory";
        case BRFS_ERR_NOT_DIRECTORY:    return "Not a directory";
        case BRFS_ERR_PATH_TOO_LONG:    return "Path too long";
        case BRFS_ERR_NAME_TOO_LONG:    return "Filename too long";
        case BRFS_ERR_INVALID_SUPERBLOCK: return "Invalid superblock";
        case BRFS_ERR_FLASH_ERROR:      return "Flash error";
        case BRFS_ERR_SEEK_ERROR:       return "Seek error";
        case BRFS_ERR_READ_ERROR:       return "Read error";
        case BRFS_ERR_WRITE_ERROR:      return "Write error";
        case BRFS_ERR_NOT_INITIALIZED:  return "Not initialized";
        default:                        return "Unknown error";
    }
}
