#ifndef BRFS_H
#define BRFS_H

/*
 * Bart's RAM File System (BRFS)
 * 
 * A FAT-based filesystem designed for the FPGC that uses RAM as a cache 
 * with SPI Flash as persistent storage. It implements a File Allocation Table
 * filesystem with support for hierarchical directories.
 * 
 * Architecture:
 * - RAM cache: Holds the active filesystem data for fast access
 * - SPI Flash: Persistent storage for data when power is off
 * - FAT: Tracks block allocation and file chain linking
 * - Directories: Special files containing directory entries
 * 
 * Memory Layout in RAM Cache:
 * [Superblock (16 words)] [FAT (total_blocks words)] [Data blocks]
 * 
 * Memory Layout in SPI Flash:
 * [Superblock sector] [FAT sectors] [Data block sectors]
 *
 * Future improvements:
 * - Allow the cache to be smaller than the persistent storage
 * - Abstracted storage backend to support SD card
 */

#include "libs/common/stddef.h"
#include "libs/kernel/mem/mem_defs.h"

typedef void (*brfs_progress_callback_t)(const char* phase, unsigned int current, unsigned int total);

//============================================================================
// Configuration Constants
//============================================================================

// BRFS Version
#define BRFS_VERSION                1

// Maximum limits
#define BRFS_MAX_PATH_LENGTH        127     // Maximum path length in characters
#define BRFS_MAX_FILENAME_LENGTH    16      // Maximum filename length (4 words * 4 chars)
#define BRFS_MAX_OPEN_FILES         16      // Maximum simultaneously open files
#define BRFS_MAX_BLOCKS             65536   // Maximum blocks in filesystem

// RAM Cache Configuration
#define BRFS_CACHE_ADDR             MEM_BRFS_CACHE_START    // Start address of RAM cache
#define BRFS_MAX_CACHE_SIZE         0x800000                // 8 MiW (32 MiB) max cache

// SPI Flash Configuration - Default addresses (in bytes)
// Superblock: 16 words = 64 bytes, stored in first sector
// FAT: stored starting at 4KB (next sector)
// Data: stored starting at 64KB
// TODO: optimize these or make them configurable based on format parameters
#define BRFS_FLASH_SUPERBLOCK_ADDR  0x00000     // Superblock at byte 0
#define BRFS_FLASH_FAT_ADDR         0x01000     // FAT starts at byte 4096 (4KB)
#define BRFS_FLASH_DATA_ADDR        0x10000     // Data blocks start at byte 65536 (64KB)

// SPI Flash Geometry
#define BRFS_FLASH_SECTOR_SIZE      4096        // 4KB sector size in bytes
#define BRFS_FLASH_PAGE_SIZE        256         // 256 byte page size
#define BRFS_FLASH_WORDS_PER_SECTOR (BRFS_FLASH_SECTOR_SIZE / 4)  // 1024 words per sector
#define BRFS_FLASH_WORDS_PER_PAGE   (BRFS_FLASH_PAGE_SIZE / 4)    // 64 words per page

// Structure sizes in words
#define BRFS_SUPERBLOCK_SIZE        16          // Superblock is 16 words
#define BRFS_DIR_ENTRY_SIZE         8           // Directory entry is 8 words

// FAT special values
#define BRFS_FAT_FREE               0           // Block is free
#define BRFS_FAT_EOF                ((unsigned int)-1)  // End of file chain

// File/Directory flags (stored in dir_entry.flags)
#define BRFS_FLAG_DIRECTORY         0x01        // Entry is a directory
#define BRFS_FLAG_HIDDEN            0x02        // Entry is hidden

//============================================================================
// Error Codes
//============================================================================

#define BRFS_OK                     0       // Success
#define BRFS_ERR_INVALID_PARAM      -1      // Invalid parameter
#define BRFS_ERR_NOT_FOUND          -2      // File or directory not found
#define BRFS_ERR_EXISTS             -3      // File or directory already exists
#define BRFS_ERR_NO_SPACE           -4      // No free blocks available
#define BRFS_ERR_NO_ENTRY           -5      // No free directory entries
#define BRFS_ERR_NOT_EMPTY          -6      // Directory not empty
#define BRFS_ERR_IS_OPEN            -7      // File is already open
#define BRFS_ERR_NOT_OPEN           -8      // File is not open
#define BRFS_ERR_TOO_MANY_OPEN      -9      // Too many open files
#define BRFS_ERR_IS_DIRECTORY       -10     // Cannot perform file operation on directory
#define BRFS_ERR_NOT_DIRECTORY      -11     // Path component is not a directory
#define BRFS_ERR_PATH_TOO_LONG      -12     // Path exceeds maximum length
#define BRFS_ERR_NAME_TOO_LONG      -13     // Filename exceeds maximum length
#define BRFS_ERR_INVALID_SUPERBLOCK -14     // Superblock validation failed
#define BRFS_ERR_FLASH_ERROR        -15     // SPI Flash operation failed
#define BRFS_ERR_SEEK_ERROR         -16     // Seek position invalid
#define BRFS_ERR_READ_ERROR         -17     // Read operation failed
#define BRFS_ERR_WRITE_ERROR        -18     // Write operation failed
#define BRFS_ERR_NOT_INITIALIZED    -19     // Filesystem not initialized

//============================================================================
// Data Structures
//============================================================================

/**
 * Superblock structure - stored at the beginning of the filesystem
 * Total size: 16 words
 */
struct brfs_superblock
{
    unsigned int total_blocks;      // Total number of blocks in filesystem
    unsigned int words_per_block;   // Words per block (e.g., 256 = 1KB blocks)
    unsigned int label[10];         // Volume label (1 char per word, null-terminated)
    unsigned int brfs_version;      // BRFS version number
    unsigned int reserved[3];       // Reserved for future use
};

/**
 * Directory entry structure
 * Total size: 8 words
 * 
 * Filename is stored compressed: 4 characters packed into each word
 * This allows 16 characters maximum filename length
 */
struct brfs_dir_entry
{
    unsigned int filename[4];       // Compressed filename (4 chars per word)
    unsigned int modify_date;       // Modification date (reserved for RTC support)
    unsigned int flags;             // Entry flags (directory, hidden, etc.)
    unsigned int fat_idx;           // Index of first block in FAT chain
    unsigned int filesize;          // File size in words (0 for empty files)
};

/**
 * Open file descriptor
 * Tracks state for each open file
 */
struct brfs_file
{
    unsigned int fat_idx;                   // FAT index of first block (0 if slot unused)
    unsigned int cursor;                    // Current read/write position in words
    struct brfs_dir_entry* dir_entry;       // Pointer to directory entry in cache
};

/**
 * Filesystem state
 * Contains all runtime state for the filesystem
 */
struct brfs_state
{
    unsigned int* cache;                            // Pointer to RAM cache
    unsigned int cache_size;                        // Size of cache in words
    unsigned int initialized;                       // Non-zero if filesystem is mounted
    unsigned int flash_id;                          // SPI Flash device ID (0 or 1)
    unsigned int flash_superblock_addr;             // Flash address of superblock
    unsigned int flash_fat_addr;                    // Flash address of FAT
    unsigned int flash_data_addr;                   // Flash address of data blocks
    struct brfs_file open_files[BRFS_MAX_OPEN_FILES];   // Open file table
    unsigned int dirty_blocks[(BRFS_MAX_BLOCKS + 31) / 32]; // Dirty block bitmap
};

//============================================================================
// Initialization Functions
//============================================================================

/**
 * Initialize the BRFS subsystem
 * Must be called before any other BRFS functions
 * 
 * @param flash_id      SPI Flash device ID (SPI_FLASH_0 or SPI_FLASH_1)
 * @return              BRFS_OK on success, error code on failure
 */
int brfs_init(unsigned int flash_id);

/**
 * Set optional progress callback for long-running operations.
 * Callback receives phase name and current/total progress counters.
 * Pass NULL to disable progress callbacks.
 */
void brfs_set_progress_callback(brfs_progress_callback_t callback);

/**
 * Format the filesystem
 * Creates a new empty filesystem, erasing all existing data
 * 
 * @param total_blocks      Number of blocks in filesystem
 * @param words_per_block   Words per block (must be multiple of 64, max 2048)
 * @param label             Volume label (max 10 characters)
 * @param full_format       If non-zero, zero-initialize all data blocks
 * @return                  BRFS_OK on success, error code on failure
 */
int brfs_format(unsigned int total_blocks, unsigned int words_per_block, 
                const char* label, int full_format);

/**
 * Mount filesystem from flash
 * Reads superblock, FAT, and data from SPI Flash into RAM cache
 * 
 * @return  BRFS_OK on success, error code on failure
 */
int brfs_mount();

/**
 * Unmount filesystem
 * Syncs any dirty data to flash and marks filesystem as unmounted
 * 
 * @return  BRFS_OK on success, error code on failure
 */
int brfs_unmount();

/**
 * Sync all dirty blocks to flash
 * Should be called periodically to ensure data persistence
 * 
 * @return  BRFS_OK on success, error code on failure
 */
int brfs_sync();

//============================================================================
// File Operations
//============================================================================

/**
 * Create a new file
 * 
 * @param path  Full path for the new file (e.g., "/dir/file.txt")
 * @return      BRFS_OK on success, error code on failure
 */
int brfs_create_file(const char* path);

/**
 * Open an existing file
 * 
 * @param path  Full path to the file
 * @return      File descriptor (>= 0) on success, error code (< 0) on failure
 */
int brfs_open(const char* path);

/**
 * Close an open file
 * 
 * @param fd    File descriptor returned by brfs_open
 * @return      BRFS_OK on success, error code on failure
 */
int brfs_close(int fd);

/**
 * Read from an open file
 * 
 * @param fd        File descriptor
 * @param buffer    Buffer to read data into
 * @param length    Number of words to read
 * @return          Number of words read (>= 0), or error code (< 0)
 */
int brfs_read(int fd, unsigned int* buffer, unsigned int length);

/**
 * Write to an open file
 * 
 * @param fd        File descriptor
 * @param buffer    Buffer containing data to write
 * @param length    Number of words to write
 * @return          Number of words written (>= 0), or error code (< 0)
 */
int brfs_write(int fd, const unsigned int* buffer, unsigned int length);

/**
 * Seek to a position in a file
 * 
 * @param fd        File descriptor
 * @param offset    New cursor position in words from start of file
 * @return          New cursor position, or error code on failure
 */
int brfs_seek(int fd, unsigned int offset);

/**
 * Get current cursor position
 * 
 * @param fd    File descriptor
 * @return      Current cursor position in words, or error code on failure
 */
int brfs_tell(int fd);

/**
 * Get file size
 * 
 * @param fd    File descriptor
 * @return      File size in words, or error code on failure
 */
int brfs_file_size(int fd);

//============================================================================
// Directory Operations
//============================================================================

/**
 * Create a new directory
 * 
 * @param path  Full path for the new directory
 * @return      BRFS_OK on success, error code on failure
 */
int brfs_create_dir(const char* path);

/**
 * Read directory entries
 * 
 * @param path      Path to directory
 * @param buffer    Buffer to store directory entries
 * @param max_entries   Maximum number of entries to read
 * @return          Number of entries read, or error code on failure
 */
int brfs_read_dir(const char* path, struct brfs_dir_entry* buffer, unsigned int max_entries);

//============================================================================
// File/Directory Management
//============================================================================

/**
 * Delete a file or empty directory
 * 
 * @param path  Path to file or directory to delete
 * @return      BRFS_OK on success, error code on failure
 */
int brfs_delete(const char* path);

/**
 * Get information about a file or directory
 * 
 * @param path      Path to file or directory
 * @param entry     Pointer to store directory entry info
 * @return          BRFS_OK on success, error code on failure
 */
int brfs_stat(const char* path, struct brfs_dir_entry* entry);

/**
 * Check if path exists
 * 
 * @param path  Path to check
 * @return      1 if exists, 0 if not
 */
int brfs_exists(const char* path);

/**
 * Check if path is a directory
 * 
 * @param path  Path to check
 * @return      1 if directory, 0 if not (or doesn't exist)
 */
int brfs_is_dir(const char* path);

//============================================================================
// Utility Functions
//============================================================================

/**
 * Get error description string
 * 
 * @param error_code    Error code from BRFS function
 * @return              Human-readable error description
 */
const char* brfs_strerror(int error_code);

/**
 * Get filesystem statistics
 * 
 * @param total_blocks  Output: total blocks in filesystem
 * @param free_blocks   Output: number of free blocks
 * @param block_size    Output: words per block
 * @return              BRFS_OK on success, error code on failure
 */
int brfs_statfs(unsigned int* total_blocks, unsigned int* free_blocks, 
                unsigned int* block_size);

/**
 * Get the mounted filesystem label.
 *
 * @param label_buffer   Output buffer for null-terminated label
 * @param buffer_size    Size of label_buffer in chars
 * @return               BRFS_OK on success, error code on failure
 */
int brfs_get_label(char* label_buffer, unsigned int buffer_size);

//============================================================================
// Internal Helper Functions (exposed for testing)
//============================================================================

/**
 * Compress a string from 1-char-per-word to 4-chars-per-word format
 * Used for storing filenames efficiently
 * 
 * @param dest      Destination buffer (must hold (strlen(src)+3)/4 words)
 * @param src       Source string (1 char per word)
 */
void brfs_compress_string(unsigned int* dest, const char* src);

/**
 * Decompress a string from 4-chars-per-word to 1-char-per-word format
 * 
 * @param dest      Destination buffer (must hold 4*src_words + 1 chars)
 * @param src       Source compressed data
 * @param src_words Number of words in source
 */
void brfs_decompress_string(char* dest, const unsigned int* src, unsigned int src_words);

/**
 * Parse a path into directory and filename components
 * 
 * @param path          Full path to parse
 * @param dir_path      Output buffer for directory path
 * @param filename      Output buffer for filename
 * @param dir_path_size Size of dir_path buffer
 * @return              BRFS_OK on success, error code on failure
 */
int brfs_parse_path(const char* path, char* dir_path, char* filename, 
                    unsigned int dir_path_size);

#endif // BRFS_H
