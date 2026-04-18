# Filesystem (BRFS v2)

BRFS — *Bart's RAM File System* — is the filesystem used by the FPGC. It
is a small, FAT-based design that uses RAM as a fast cache over a
pluggable persistent storage backend (today: SPI flash).

## Overview

BRFS v2 has the following characteristics:

- **FAT-based** — A File Allocation Table tracks block allocation and
  per-file chains.
- **RAM-cached** — Blocks live in a contiguous in-RAM cache for fast
  access. The whole filesystem currently fits inside the cache (the
  LRU eviction layer is planned for v2.1 alongside SD-card support).
- **Pluggable storage backend** — A small `brfs_storage_t` vtable
  abstracts the persistent medium. Only the SPI-flash backend ships
  today; the SD-card backend is a v2.1 follow-up.
- **Byte-native API** — `brfs_read` / `brfs_write` / `brfs_seek` and the
  on-disk `filesize` field are byte-counted. Internally the cache still
  reads/writes whole blocks; partial-block writes are handled
  transparently.
- **Hierarchical directories** with up to 16-character filenames.
- **Little-endian on disk** (matches the b32p3 CPU and the host
  tooling).
- **Persistence on demand** — Data is written back to flash only when
  `brfs_sync()` is called (or the kernel calls it on shutdown / from
  the `sync` shell built-in). Between syncs the FS lives entirely in
  RAM; after a power cycle the volume rolls back to the last synced
  state.

## Architecture

BRFS v2 is split into the layers described in §4.1 of the plan:

```
+-----------------------------------------------------+
| BDOS VFS  (Software/C/bdos/vfs.c file_dev_table)    |
|   bdos_vfs_open / read / write / close / lseek      |
+-----------------------------------------------------+
| BRFS v2 core   (Software/C/libfpgc/fs/brfs.c)       |
|   path walk · directory ops · FAT · file ops        |
+-----------------------------------------------------+
| Block cache    (Software/C/libfpgc/fs/brfs_cache.c) |
|   linear-pinned today; LRU pool in v2.1             |
+-----------------------------------------------------+
| Storage backend vtable  (brfs_storage_t)            |
|   spi_flash today; sd_card and ram-test in v2.1     |
+-----------------------------------------------------+
| Hardware drivers  (Software/C/libfpgc/io/spi_flash.c, …) |
+-----------------------------------------------------+
```

The BDOS VFS bridge is a thin pass-through: there is no byte-buffering
layer above BRFS.

### Memory layout

BRFS organises data in three regions, mirrored between the in-RAM cache
and the persistent backend:

```
┌──────────────────────────────────────────────────┐
│  Superblock (16 words)                           │
│  - Filesystem metadata and configuration         │
├──────────────────────────────────────────────────┤
│  FAT (File Allocation Table)                     │
│  - One 32-bit entry per data block               │
├──────────────────────────────────────────────────┤
│  Data blocks                                     │
│  - File and directory content (FAT-chained)      │
└──────────────────────────────────────────────────┘
```

The on-disk magic is `BRF2` and the superblock `version` field is `2`.
v1 volumes are not readable by v2 — the volume must be reformatted
(see the format wizard / `/bin/format` below).

### Superblock

The superblock is 16 words (64 bytes):

| Field | Size (words) | Description |
|-------|--------------|-------------|
| `total_blocks` | 1 | Total number of data blocks |
| `words_per_block` | 1 | Words per block (e.g. `1024` = 4 KiB) |
| `label` | 10 | Volume label (one ASCII char per word, NUL-terminated) |
| `brfs_version` | 1 | Filesystem version (`2` for BRFS v2) |
| `reserved` | 3 | Reserved for future use |

### File Allocation Table (FAT)

The FAT is an array where each entry corresponds to one data block:

- `0` — Block is free.
- `0xFFFFFFFF` — End-of-chain marker (EOF).
- Any other value — Index of the next block in the chain.

Files larger than one block are stored as linked chains in the FAT. A
read walks the chain from the first block (recorded in the directory
entry) until it hits the EOF marker. The FAT is permanently pinned in
the cache so path walks and seeks never miss.

### Directory entries

Directories are special files holding fixed-size 8-word entries:

| Field | Size (words) | Description |
|-------|--------------|-------------|
| `filename` | 4 | Compressed filename (4 chars / word, 16 chars max) |
| `modify_date` | 1 | Modification date (reserved for RTC) |
| `flags` | 1 | Entry flags (directory, hidden) |
| `fat_idx` | 1 | Index of the first block in the FAT |
| `filesize` | 1 | **File size in bytes** (changed from words in v1) |

A 4 KiB block holds 128 directory entries. Block 0 of the data region
is always the root directory; it is initialised during `brfs_format()`
with `.` and `..` entries pointing to itself.

## Storage backends

Storage backends can be added, in contrast to V1 where SPI flash was tightly coupled to the FS code.

Concrete backends:

- **`brfs_storage_spi_flash`** — wraps `spi_flash_read_words` /
  `spi_flash_write_words` / `spi_flash_erase_sector`. `block_size = 4096`,
  `erase_unit_blocks = 1`. This is the production backend used by BDOS.
- **SD-card backend** — designed for v2.1 once a real
  `Software/C/libfpgc/io/sd.c` driver lands. `block_size = 512`,
  no erase needed.
- **RAM backend** — small in-memory backend for host-side unit tests
  of the cache and FS core. Not exposed in BDOS.

## Cache

In v2.0 the cache is a single contiguous buffer sized for the entire
mounted filesystem (`superblock + FAT + data_blocks * block_size`).
On the current 4 MiB SPI-flash partition this is ~4 MiB of BDOS heap,
well within budget, and every block is effectively pinned —
`brfs_cache_get_data_block(blk)` returns a pointer that stays valid
for the lifetime of the mount. This keeps the v1 "everything in RAM"
performance characteristics while the new vtable + byte-native API
shipped underneath. An LRU cache will be added in v2.1.

### Dirty-block tracking

BRFS uses a bitmap to record which blocks have changed since the last
sync. `brfs_sync()` walks the bitmap and writes only dirty blocks back
to the storage backend. The sync also issues the appropriate
sector-erase commands on the flash backend before writing, since SPI
flash can only be programmed into erased pages.

## Persistence and the `sync` model

BRFS v2 deliberately keeps v1's deferred-flush model:

- All reads and writes hit the in-RAM cache immediately (so they are
  fast).
- Mutations mark the affected blocks dirty.
- Persistent storage is updated only when `brfs_sync()` is called —
  either explicitly via the shell `sync` built-in, or implicitly by
  certain operations (e.g. `format`).

This means a power loss between syncs rolls the volume back to the
last synced state. That trade-off — recoverability over durability —
is what makes the FS feel snappy on slow SPI flash. It is documented
behaviour, not a bug; users running long write sessions should
periodically `sync`.

## Flash write strategy

SPI flash imposes hard constraints that the SPI-flash backend handles
internally:

1. Reads can be any size from any address.
2. Writes must target erased memory and are done in 256-byte page
   chunks (max 64 words at a time).
3. Erases are 4 KiB sector-aligned (1024 words / one BRFS block).

BRFS therefore picks `block_size = 4096` so one filesystem block lines
up with one flash sector. The format wizard enforces that
`words_per_block` is a multiple of 64 and that `total_blocks` is a
multiple of 64 to keep these alignments simple.

## Formatting

A volume is formatted with `brfs_format(total_blocks, words_per_block, label)`.
There are two ways to invoke it from a running system:

- **Interactive boot wizard.** If BDOS fails to mount the filesystem
  on boot (no valid superblock, wrong magic), it drops into a
  stripped-down prompt that walks the user through entering block
  count, bytes-per-block, and a label. This path lives in
  `Software/C/bdos/shell_format.c` and is the only way to format from
  a system that does not yet have an FS to load `/bin/format` from.
- **`/bin/format` userBDOS program.** From a healthy system,
  `format <blocks> <bytes-per-block> <label>` runs the same code path
  via `SYSCALL_FS_FORMAT` (40). This replaces the v1 in-shell `format`
  built-in.

For a 4 MiB partition the proper settings are
`1024` blocks × `4096` bytes per block.

## Limitations

- 16-character filenames maximum.
- 127-character paths maximum.
- No file permissions, owners, or ACLs.
- No timestamps (RTC support reserved but not implemented).
- Single-user, single-foreground; no locking.
- The v2.0 cache requires the entire FS to fit in the BDOS heap budget
  (~4 MiB today). The v2.1 LRU cache lifts this limit.
