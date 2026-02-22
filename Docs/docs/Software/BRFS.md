# Filesystem (BRFS)

BRFS (Bart's RAM File System) is a FAT-based filesystem designed for the FPGC that uses RAM as a cache with SPI Flash (currently, but in the future SD card might also be supported) as persistent storage. It provides hierarchical directory support with a simple design optimized for the word-addressable B32P3 architecture.

## Overview

BRFS has the following characteristics:

- **FAT-based** - Uses a File Allocation Table to manage block allocation and file chains
- **RAM-cached operation** - The entire filesystem is loaded into RAM for fast access since the FPGC has plenty of RAM
- **SPI Flash persistence** - Data is persisted to SPI Flash for non-volatile storage (explicitly only via `brfs_sync` function)
- **Hierarchical directories** - Supports nested directories
- **Word-aligned storage** - All data stored as 32-bit words (native to B32P3)
- **Compressed filenames** - Max 16-character filenames packed into 4 words

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
│  - Block size configurable                       │
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

A 1024-word block can hold 128 directory entries.

## Implementation Notes

**Filename Compression:**

Filenames are stored with 4 characters packed per 32-bit word to save space.
This allows 16-character filenames in just 4 words.

**Dirty Block Tracking:**

BRFS uses a bitmap to track which blocks have been modified since the last sync. Only dirty blocks are written to flash during `brfs_sync()`.

**Flash Write Strategy:**

Due to SPI Flash constraints:

1. **Reads** can be any size from any address
2. **Writes** must be to erased memory, max 256 bytes (64 words) per page
3. **Erases** are 4KB sectors (1024 words) minimum

BRFS handles this by:

- Erasing entire sectors before writing
- Writing data in 64-word page chunks
- Words per block should be a multiple of 64 to align with flash page size

### Root Directory

Block 0 is always reserved for the root directory. It is initialized during `brfs_format()` with `.` and `..` entries pointing to itself.

## Limitations

- Maximum 16-character filenames
- Maximum 127-character paths
- No file permissions or ownership
- No timestamps (RTC support reserved but not implemented)
- Single-user, no locking
- Cache must fit entire filesystem in RAM

Most of these limitations are intentional design choices to keep the filesystem simple and efficient for the FPGC's architecture and use cases, but in the future I might want to add something to support larger filesystem sizes with dynamic caching.

## Formatting parameters

The number of blocks and block size should be a multiple of 64. The size of the superblock and FAT need to be taken into account when calculating total flash usage. For example, formatting an SPI Flash of 16MiB with 256-word blocks can best be done with 16320 blocks.
