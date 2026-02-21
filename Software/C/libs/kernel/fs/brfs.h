#ifndef BRFS_H
#define BRFS_H

// Bart's RAM File System (BRFS)
// A FAT-based filesystem designed for the FPGC that uses RAM as a cache
// with SPI Flash as persistent storage. It implements a File Allocation Table
// filesystem with support for hierarchical directories.
// Architecture:
// - RAM cache: Holds the active filesystem data for fast access
// - SPI Flash: Persistent storage for data when power is off
// - FAT: Tracks block allocation and file chain linking
// - Directories: Special files containing directory entries
// Memory Layout in RAM Cache:
// [Superblock (16 words)] [FAT (total_blocks words)] [Data blocks]
// Memory Layout in SPI Flash:
// [Superblock sector] [FAT sectors] [Data block sectors]
// Future improvements:
// - Allow the cache to be smaller than the persistent storage
// - Abstracted storage backend to support SD card

#include "libs/common/stddef.h"

typedef void (*brfs_progress_callback_t)(const char *phase, unsigned int current, unsigned int total);

// ---- Configuration Constants ----
// BRFS Version
#define BRFS_VERSION 1

// Maximum limits
#define BRFS_MAX_PATH_LENGTH 127    // Maximum path length in characters
#define BRFS_MAX_FILENAME_LENGTH 16 // Maximum filename length (4 words * 4 chars)
#define BRFS_MAX_OPEN_FILES 16      // Maximum simultaneously open files
#define BRFS_MAX_BLOCKS 65536       // Maximum blocks in filesystem

// SPI Flash Configuration - Default addresses (in bytes)
// Superblock: 16 words = 64 bytes, stored in first sector
// FAT: stored starting at 4KB (next sector)
// Data: stored starting at 64KB
// TODO: optimize these or make them configurable based on format parameters
#define BRFS_FLASH_SUPERBLOCK_ADDR 0x00000 // Superblock at byte 0
#define BRFS_FLASH_FAT_ADDR 0x01000        // FAT starts at byte 4096 (4KB)
#define BRFS_FLASH_DATA_ADDR 0x10000       // Data blocks start at byte 65536 (64KB)

// SPI Flash Geometry
#define BRFS_FLASH_SECTOR_SIZE 4096                              // 4KB sector size in bytes
#define BRFS_FLASH_PAGE_SIZE 256                                 // 256 byte page size
#define BRFS_FLASH_WORDS_PER_SECTOR (BRFS_FLASH_SECTOR_SIZE / 4) // 1024 words per sector
#define BRFS_FLASH_WORDS_PER_PAGE (BRFS_FLASH_PAGE_SIZE / 4)     // 64 words per page

// Structure sizes in words
#define BRFS_SUPERBLOCK_SIZE 16 // Superblock is 16 words
#define BRFS_DIR_ENTRY_SIZE 8   // Directory entry is 8 words

// FAT special values
#define BRFS_FAT_FREE 0                 // Block is free
#define BRFS_FAT_EOF ((unsigned int)-1) // End of file chain

// File/Directory flags (stored in dir_entry.flags)
#define BRFS_FLAG_DIRECTORY 0x01 // Entry is a directory
#define BRFS_FLAG_HIDDEN 0x02    // Entry is hidden

// ---- Error Codes ----
#define BRFS_OK 0                       // Success
#define BRFS_ERR_INVALID_PARAM -1       // Invalid parameter
#define BRFS_ERR_NOT_FOUND -2           // File or directory not found
#define BRFS_ERR_EXISTS -3              // File or directory already exists
#define BRFS_ERR_NO_SPACE -4            // No free blocks available
#define BRFS_ERR_NO_ENTRY -5            // No free directory entries
#define BRFS_ERR_NOT_EMPTY -6           // Directory not empty
#define BRFS_ERR_IS_OPEN -7             // File is already open
#define BRFS_ERR_NOT_OPEN -8            // File is not open
#define BRFS_ERR_TOO_MANY_OPEN -9       // Too many open files
#define BRFS_ERR_IS_DIRECTORY -10       // Cannot perform file operation on directory
#define BRFS_ERR_NOT_DIRECTORY -11      // Path component is not a directory
#define BRFS_ERR_PATH_TOO_LONG -12      // Path exceeds maximum length
#define BRFS_ERR_NAME_TOO_LONG -13      // Filename exceeds maximum length
#define BRFS_ERR_INVALID_SUPERBLOCK -14 // Superblock validation failed
#define BRFS_ERR_FLASH_ERROR -15        // SPI Flash operation failed
#define BRFS_ERR_SEEK_ERROR -16         // Seek position invalid
#define BRFS_ERR_READ_ERROR -17         // Read operation failed
#define BRFS_ERR_WRITE_ERROR -18        // Write operation failed
#define BRFS_ERR_NOT_INITIALIZED -19    // Filesystem not initialized

// ---- Data Structures ----
// Superblock structure - stored at the beginning of the filesystem
// Total size: 16 words
struct brfs_superblock
{
  unsigned int total_blocks;    // Total number of blocks in filesystem
  unsigned int words_per_block; // Words per block (e.g., 256 = 1KB blocks)
  unsigned int label[10];       // Volume label (1 char per word, null-terminated)
  unsigned int brfs_version;    // BRFS version number
  unsigned int reserved[3];     // Reserved for future use
};

// Directory entry structure
// Total size: 8 words
// Filename is stored compressed: 4 characters packed into each word
// This allows 16 characters maximum filename length
struct brfs_dir_entry
{
  unsigned int filename[4]; // Compressed filename (4 chars per word)
  unsigned int modify_date; // Modification date (reserved for RTC support)
  unsigned int flags;       // Entry flags (directory, hidden, etc.)
  unsigned int fat_idx;     // Index of first block in FAT chain
  unsigned int filesize;    // File size in words (0 for empty files)
};

// Open file descriptor
// Tracks state for each open file
struct brfs_file
{
  unsigned int fat_idx;             // FAT index of first block (0 if slot unused)
  unsigned int cursor;              // Current read/write position in words
  struct brfs_dir_entry *dir_entry; // Pointer to directory entry in cache
};

// Filesystem state
// Contains all runtime state for the filesystem
struct brfs_state
{
  unsigned int *cache;                                    // Pointer to RAM cache
  unsigned int cache_size;                                // Size of cache in words
  unsigned int initialized;                               // Non-zero if filesystem is mounted
  unsigned int flash_id;                                  // SPI Flash device ID (0 or 1)
  unsigned int flash_superblock_addr;                     // Flash address of superblock
  unsigned int flash_fat_addr;                            // Flash address of FAT
  unsigned int flash_data_addr;                           // Flash address of data blocks
  struct brfs_file open_files[BRFS_MAX_OPEN_FILES];       // Open file table
  unsigned int dirty_blocks[(BRFS_MAX_BLOCKS + 31) / 32]; // Dirty block bitmap
};

// ---- Initialization Functions ----
// Initialize the BRFS subsystem
// Must be called before any other BRFS functions
int brfs_init(unsigned int flash_id, unsigned int *cache_addr, unsigned int cache_size);

// Set optional progress callback for long-running operations.
// Callback receives phase name and current/total progress counters.
// Pass NULL to disable progress callbacks.
void brfs_set_progress_callback(brfs_progress_callback_t callback);

// Format the filesystem
// Creates a new empty filesystem, erasing all existing data
int brfs_format(unsigned int total_blocks, unsigned int words_per_block,
                const char *label, int full_format);

// Mount filesystem from flash
// Reads superblock, FAT, and data from SPI Flash into RAM cache
int brfs_mount();

// Unmount filesystem
// Syncs any dirty data to flash and marks filesystem as unmounted
int brfs_unmount();

// Sync all dirty blocks to flash
// Should be called periodically to ensure data persistence
int brfs_sync();

// ---- File Operations ----
// Create a new file
int brfs_create_file(const char *path);

// Open an existing file
int brfs_open(const char *path);

// Close an open file
int brfs_close(int fd);

// Read from an open file
int brfs_read(int fd, unsigned int *buffer, unsigned int length);

// Write to an open file
int brfs_write(int fd, const unsigned int *buffer, unsigned int length);

// Seek to a position in a file
int brfs_seek(int fd, unsigned int offset);

// Get current cursor position
int brfs_tell(int fd);

// Get file size
int brfs_file_size(int fd);

// ---- Directory Operations ----
// Create a new directory
int brfs_create_dir(const char *path);

// Read directory entries
int brfs_read_dir(const char *path, struct brfs_dir_entry *buffer, unsigned int max_entries);

// ---- File/Directory Management ----
// Delete a file or empty directory
int brfs_delete(const char *path);

// Get information about a file or directory
int brfs_stat(const char *path, struct brfs_dir_entry *entry);

// Check if path exists
int brfs_exists(const char *path);

// Check if path is a directory
int brfs_is_dir(const char *path);

// ---- Utility Functions ----
// Get error description string
const char *brfs_strerror(int error_code);

// Get filesystem statistics
int brfs_statfs(unsigned int *total_blocks, unsigned int *free_blocks,
                unsigned int *block_size);

// Get the mounted filesystem label.
int brfs_get_label(char *label_buffer, unsigned int buffer_size);

// ---- Internal Helper Functions (exposed for testing) ----
// Compress a string from 1-char-per-word to 4-chars-per-word format
// Used for storing filenames efficiently
void brfs_compress_string(unsigned int *dest, const char *src);

// Decompress a string from 4-chars-per-word to 1-char-per-word format
void brfs_decompress_string(char *dest, const unsigned int *src, unsigned int src_words);

// Parse a path into directory and filename components
int brfs_parse_path(const char *path, char *dir_path, char *filename,
                    unsigned int dir_path_size);

#endif // BRFS_H
