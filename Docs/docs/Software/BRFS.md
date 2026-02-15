# BRFS Filesystem

BRFS (Bart's RAM File System) is a FAT-based filesystem designed for the FPGC that uses RAM as a cache with SPI Flash as persistent storage. It provides hierarchical directory support with a simple, efficient design optimized for the word-addressable B32P3 architecture.

## Overview

BRFS implements a File Allocation Table (FAT) filesystem with the following characteristics:

- **RAM-cached operation** - The entire filesystem is loaded into RAM for fast access
- **SPI Flash persistence** - Data is persisted to SPI Flash for non-volatile storage
- **Hierarchical directories** - Full support for nested directory structures
- **Word-aligned storage** - All data stored as 32-bit words (native to B32P3)
- **Compressed filenames** - 16-character filenames packed into 4 words

## Architecture

### Memory Layout

BRFS organizes data in three regions, both in RAM cache and SPI Flash:

```
┌──────────────────────────────────────────────────┐
│  Superblock (16 words)                           │
│  - Filesystem metadata and configuration         │
├──────────────────────────────────────────────────┤
│  FAT (File Allocation Table)                     │
│  - One entry per block (total_blocks words)      │
│  - Tracks block allocation and file chains       │
├──────────────────────────────────────────────────┤
│  Data Blocks                                     │
│  - File and directory content                    │
│  - Block size configurable (default 1024 words)  │
└──────────────────────────────────────────────────┘
```

### Superblock Structure

The superblock (16 words) contains filesystem metadata:

| Field | Size (words) | Description |
|-------|--------------|-------------|
| `total_blocks` | 1 | Total number of blocks in filesystem |
| `words_per_block` | 1 | Words per block (e.g., 256 = 1KB) |
| `label` | 10 | Volume label (1 char per word) |
| `brfs_version` | 1 | BRFS version number |
| `reserved` | 3 | Reserved for future use |

### File Allocation Table (FAT)

The FAT is an array where each entry corresponds to a data block:

- `0` - Block is free
- `0xFFFFFFFF` - End of file chain (EOF)
- Any other value - Index of next block in chain

Files larger than one block are stored as linked chains in the FAT. To read a file, follow the chain from the first block (stored in the directory entry) until reaching EOF.

### Directory Entries

Directories are special files containing 8-word entries:

| Field | Size (words) | Description |
|-------|--------------|-------------|
| `filename` | 4 | Compressed filename (4 chars/word, 16 chars max) |
| `modify_date` | 1 | Modification date (reserved for RTC) |
| `flags` | 1 | Entry flags (directory, hidden) |
| `fat_idx` | 1 | Index of first block in FAT |
| `filesize` | 1 | File size in words |

Each 1024-word block can hold 128 directory entries.

## Configuration

### Flash Geometry

BRFS is designed around SPI Flash characteristics:

- **Sector size**: 4KB (1024 words) - minimum erase unit
- **Page size**: 256 bytes (64 words) - maximum write unit
- **Block size**: Must be multiple of 64 words, maximum 2048 words

## API Reference

### Initialization

#### `brfs_init`

Initialize the BRFS subsystem. Must be called before any other BRFS functions.

```c
int brfs_init(unsigned int flash_id);
```

**Parameters:**

- `flash_id` - SPI Flash device ID (0 or 1)

**Returns:** `BRFS_OK` on success, error code on failure.

#### `brfs_format`

Create a new empty filesystem, erasing all existing data.

```c
int brfs_format(unsigned int total_blocks, unsigned int words_per_block, 
                const char* label, int full_format);
```

**Parameters:**

- `total_blocks` - Number of blocks in filesystem
- `words_per_block` - Words per block (multiple of 64, max 2048)
- `label` - Volume label (max 10 characters)
- `full_format` - If non-zero, zero-initialize all data blocks

**Returns:** `BRFS_OK` on success, error code on failure.

#### `brfs_mount`

Mount filesystem from flash into RAM cache.

```c
int brfs_mount();
```

**Returns:** `BRFS_OK` on success, error code on failure.

#### `brfs_unmount`

Sync dirty data to flash and unmount filesystem.

```c
int brfs_unmount();
```

**Returns:** `BRFS_OK` on success, error code on failure.

#### `brfs_sync`

Sync all dirty blocks to flash without unmounting.

```c
int brfs_sync();
```

**Returns:** `BRFS_OK` on success, error code on failure.

### File Operations

#### `brfs_create_file`

Create a new file.

```c
int brfs_create_file(const char* path);
```

**Parameters:**

- `path` - Full path for new file (e.g., `/dir/file.txt`)

**Returns:** `BRFS_OK` on success, error code on failure.

#### `brfs_open`

Open an existing file for reading/writing.

```c
int brfs_open(const char* path);
```

**Parameters:**

- `path` - Full path to file

**Returns:** File descriptor (≥ 0) on success, error code (< 0) on failure.

#### `brfs_close`

Close an open file.

```c
int brfs_close(int fd);
```

**Parameters:**

- `fd` - File descriptor from `brfs_open`

**Returns:** `BRFS_OK` on success, error code on failure.

#### `brfs_read`

Read data from an open file.

```c
int brfs_read(int fd, unsigned int* buffer, unsigned int length);
```

**Parameters:**

- `fd` - File descriptor
- `buffer` - Buffer to read data into
- `length` - Number of words to read

**Returns:** Number of words read (≥ 0), or error code (< 0).

#### `brfs_write`

Write data to an open file.

```c
int brfs_write(int fd, const unsigned int* buffer, unsigned int length);
```

**Parameters:**

- `fd` - File descriptor
- `buffer` - Data to write
- `length` - Number of words to write

**Returns:** Number of words written (≥ 0), or error code (< 0).

#### `brfs_seek`

Seek to a position in a file.

```c
int brfs_seek(int fd, unsigned int offset);
```

**Parameters:**

- `fd` - File descriptor
- `offset` - New cursor position in words from start

**Returns:** New cursor position, or error code on failure.

#### `brfs_tell`

Get current cursor position.

```c
int brfs_tell(int fd);
```

**Returns:** Current position in words, or error code on failure.

#### `brfs_file_size`

Get file size.

```c
int brfs_file_size(int fd);
```

**Returns:** File size in words, or error code on failure.

### Directory Operations

#### `brfs_create_dir`

Create a new directory.

```c
int brfs_create_dir(const char* path);
```

**Parameters:**

- `path` - Full path for new directory

**Returns:** `BRFS_OK` on success, error code on failure.

#### `brfs_read_dir`

Read directory entries.

```c
int brfs_read_dir(const char* path, struct brfs_dir_entry* buffer, 
                  unsigned int max_entries);
```

**Parameters:**

- `path` - Path to directory
- `buffer` - Buffer to store directory entries
- `max_entries` - Maximum entries to read

**Returns:** Number of entries read, or error code on failure.

### File/Directory Management

#### `brfs_delete`

Delete a file or empty directory.

```c
int brfs_delete(const char* path);
```

**Returns:** `BRFS_OK` on success, error code on failure.

#### `brfs_stat`

Get information about a file or directory.

```c
int brfs_stat(const char* path, struct brfs_dir_entry* entry);
```

**Returns:** `BRFS_OK` on success, error code on failure.

#### `brfs_exists`

Check if path exists.

```c
int brfs_exists(const char* path);
```

**Returns:** 1 if exists, 0 if not.

#### `brfs_is_dir`

Check if path is a directory.

```c
int brfs_is_dir(const char* path);
```

**Returns:** 1 if directory, 0 if not.

### Utility Functions

#### `brfs_strerror`

Get human-readable error description.

```c
const char* brfs_strerror(int error_code);
```

**Returns:** Error description string.

#### `brfs_statfs`

Get filesystem statistics.

```c
int brfs_statfs(unsigned int* total_blocks, unsigned int* free_blocks, 
                unsigned int* block_size);
```

**Returns:** `BRFS_OK` on success, error code on failure.

## Error Codes

| Code | Name | Description |
|------|------|-------------|
| 0 | `BRFS_OK` | Success |
| -1 | `BRFS_ERR_INVALID_PARAM` | Invalid parameter |
| -2 | `BRFS_ERR_NOT_FOUND` | File or directory not found |
| -3 | `BRFS_ERR_EXISTS` | File or directory already exists |
| -4 | `BRFS_ERR_NO_SPACE` | No free blocks available |
| -5 | `BRFS_ERR_NO_ENTRY` | No free directory entries |
| -6 | `BRFS_ERR_NOT_EMPTY` | Directory not empty |
| -7 | `BRFS_ERR_IS_OPEN` | File is already open |
| -8 | `BRFS_ERR_NOT_OPEN` | File is not open |
| -9 | `BRFS_ERR_TOO_MANY_OPEN` | Too many open files |
| -10 | `BRFS_ERR_IS_DIRECTORY` | Cannot perform file operation on directory |
| -11 | `BRFS_ERR_NOT_DIRECTORY` | Path component is not a directory |
| -12 | `BRFS_ERR_PATH_TOO_LONG` | Path exceeds maximum length |
| -13 | `BRFS_ERR_NAME_TOO_LONG` | Filename exceeds 16 characters |
| -14 | `BRFS_ERR_INVALID_SUPERBLOCK` | Superblock validation failed |
| -15 | `BRFS_ERR_FLASH_ERROR` | SPI Flash operation failed |
| -16 | `BRFS_ERR_SEEK_ERROR` | Seek position invalid |
| -17 | `BRFS_ERR_READ_ERROR` | Read operation failed |
| -18 | `BRFS_ERR_WRITE_ERROR` | Write operation failed |
| -19 | `BRFS_ERR_NOT_INITIALIZED` | Filesystem not initialized |

## Usage Example

```c
#define KERNEL_BRFS
#include "libs/kernel/kernel.h"

int main() {
    unsigned int data[256];
    int fd;
    int bytes_written;
    
    // Initialize and mount
    brfs_init(0);  // Use SPI Flash 0
    brfs_mount();
    
    // Create and write to a file
    brfs_create_file("/myfile.txt");
    fd = brfs_open("/myfile.txt");
    
    data[0] = 'H';
    data[1] = 'e';
    data[2] = 'l';
    data[3] = 'l';
    data[4] = 'o';
    data[5] = 0;
    
    bytes_written = brfs_write(fd, data, 6);
    brfs_close(fd);
    
    // Create a directory
    brfs_create_dir("/subdir");
    brfs_create_file("/subdir/nested.dat");
    
    // Sync to flash
    brfs_sync();
    
    // Unmount when done
    brfs_unmount();
    
    return 0;
}
```

## Implementation Notes

### Filename Compression

Filenames are stored with 4 characters packed per 32-bit word to save space:

```
Word layout: [char0 (bits 31-24)] [char1 (bits 23-16)] [char2 (bits 15-8)] [char3 (bits 7-0)]
```

This allows 16-character filenames in just 4 words (16 bytes).

### Dirty Block Tracking

BRFS uses a bitmap to track which blocks have been modified since the last sync. Only dirty blocks are written to flash during `brfs_sync()`, minimizing flash wear and improving performance.

### Flash Write Strategy

Due to SPI Flash constraints:

1. **Reads** can be any size from any address
2. **Writes** must be to erased memory, max 256 bytes (64 words) per page
3. **Erases** are 4KB sectors (1024 words) minimum

BRFS handles this by:

- Erasing entire sectors before writing
- Writing data in 64-word page chunks
- Caching all operations in RAM for fast access

### Root Directory

Block 0 is always reserved for the root directory. It is initialized during `brfs_format()` with `.` and `..` entries pointing to itself.

## Limitations

- Maximum 16-character filenames
- Maximum 127-character paths
- No file permissions or ownership
- No timestamps (RTC support reserved but not implemented)
- Single-user, no locking
- Cache must fit entire filesystem in RAM

## Formatting parameters

The number of blocks and block size should be a multiple of 64. The size of the superblock and FAT need to be taken into account when calculating total flash usage. For example, formatting an SPI Flash of 16MiB with 256-word blocks can best be done with 16320 blocks.
