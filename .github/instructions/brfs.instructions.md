---
name: 'BRFS Filesystem'
description: 'Rules for editing the BRFS filesystem implementation'
applyTo: 'Software/C/libfpgc/fs/**'
---
# BRFS filesystem guidelines

## Validation
```
make compile-kernel    # Verify kernel compiles
make test-host       # Run host-side BRFS unit tests
```

## Architecture
BRFS is a simple block-based filesystem with two independent instances:

| Instance | Mount point | Storage backend | Cache |
|----------|------------|-----------------|-------|
| `brfs_spi` | `/` (root) | SPI flash (SPI0/SPI1) | 16 MiB in-memory |
| `brfs_sd` | `/sdcard` | SD card (SPI5) | 4 MiB LRU cache |

## File map
| File | Purpose |
|------|---------|
| `brfs.c` | Core filesystem: superblock, FAT, directory entries, block I/O |
| `brfs_cache.c` | LRU block cache (used by SD backend) |
| `brfs_storage_spi_flash.c` | Storage vtable for SPI flash |
| `brfs_storage_sdcard.c` | Storage vtable for SD card |

## Storage vtable pattern
Each backend implements `brfs_storage_ops`:
```c
typedef struct {
    int (*read_block)(void *ctx, unsigned int block, void *buf);
    int (*write_block)(void *ctx, unsigned int block, const void *buf);
    int (*sync)(void *ctx);
} brfs_storage_ops;
```

## On-disk layout
```
[Superblock (1 block)] [FAT (N blocks)] [Data blocks...]
```
- Block size: configurable (typically 512 bytes for SD, larger for flash)
- FAT entries: 32-bit block pointers (0 = free, 0xFFFFFFFF = end-of-chain)
- Directory entries: fixed-size records with name, size, flags, first block

## Dangerous — ask user first
- Changing the superblock format
- Changing FAT entry encoding
- Changing directory entry structure
- Any change to `brfs.c` that affects on-disk layout

## Ripple effects
- Changing `brfs.c` public API → update `vfs.c`, `fs.c`
- Adding a new storage backend → implement vtable, register in `fs.c`/`init.c`
- Changing cache behavior → may affect SD card write performance

## Reference implementation
To add a new storage backend, study `brfs_storage_sdcard.c` — it shows
the vtable pattern, block read/write via SPI, and cache integration.
