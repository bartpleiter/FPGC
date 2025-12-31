# BDOS V2 File System Integration

**Prepared by: James O'Brien (File Systems & Storage Specialist)**  
**Contributors: Marcus Rodriguez**  
**Date: December 2024**  
**Version: 1.1**

*Revision 1.1: Updated for 64 MiB memory layout (BRFS at 0x0800000)*

---

## 1. Overview

This document describes how BDOS V2 integrates with the BRFS (Bart's RAM File System) and how file system services are provided to user programs.

---

## 2. BRFS Architecture Review

### 2.1 Current BRFS Design

Based on the existing implementation in `Software/C/libs/kernel/fs/brfs.h`:

- **FAT-based filesystem** with linked block allocation
- **RAM cache**: Full filesystem loaded into RAM for fast access
- **SPI Flash persistence**: Data persisted to SPI Flash
- **Hierarchical directories**: Standard directory tree structure

### 2.2 Key BRFS Structures

```c
// Superblock (16 words)
struct brfs_superblock {
    unsigned int total_blocks;
    unsigned int words_per_block;
    unsigned int label[10];
    unsigned int brfs_version;
    unsigned int reserved[3];
};

// Directory entry (8 words)
struct brfs_dir_entry {
    unsigned int filename[4];   // Compressed: 4 chars per word
    unsigned int modify_date;
    unsigned int flags;
    unsigned int fat_idx;
    unsigned int filesize;
};
```

### 2.3 Memory Layout

```
BRFS RAM Cache (at MEM_BRFS_START = 0x0800000):
+------------------+
|   Superblock     |  16 words
+------------------+
|   FAT Table      |  total_blocks words
+------------------+
|   Data Blocks    |  total_blocks × words_per_block
+------------------+
```

---

## 3. BDOS V2 File System Layer

### 3.1 Virtual File System (VFS) Concept

While a full VFS may be overkill, a thin abstraction layer helps:

```c
// kernel/fs/vfs.h

#ifndef VFS_H
#define VFS_H

// File flags for open()
#define O_READ      0x01
#define O_WRITE     0x02
#define O_RDWR      (O_READ | O_WRITE)
#define O_CREATE    0x04
#define O_TRUNC     0x08
#define O_APPEND    0x10

// Seek origins
#define SEEK_SET    0
#define SEEK_CUR    1
#define SEEK_END    2

// File types
#define FT_REGULAR  0
#define FT_DIR      1

// Stat structure
struct stat {
    unsigned int st_size;       // File size in words
    unsigned int st_flags;      // File flags
    unsigned int st_mtime;      // Modification time
    unsigned int st_type;       // File type (FT_REGULAR, FT_DIR)
};

// Directory entry for readdir
struct dirent {
    char name[17];              // Filename (16 chars + null)
    unsigned int size;          // File size
    unsigned int flags;         // Entry flags
    unsigned int type;          // Entry type
};

// File operations
int fs_init(void);
int fs_open(const char* path, int flags);
int fs_close(int fd);
int fs_read(int fd, void* buf, unsigned int count);
int fs_write(int fd, const void* buf, unsigned int count);
int fs_seek(int fd, int offset, int whence);
int fs_tell(int fd);
int fs_stat(const char* path, struct stat* st);
int fs_mkdir(const char* path);
int fs_rmdir(const char* path);
int fs_unlink(const char* path);
int fs_rename(const char* oldpath, const char* newpath);
int fs_opendir(const char* path);
int fs_readdir(int fd, struct dirent* entry);
int fs_closedir(int fd);
int fs_sync(void);
int fs_getcwd(char* buf, unsigned int size);
int fs_chdir(const char* path);

#endif // VFS_H
```

### 3.2 File Descriptor Table

The kernel maintains a global file descriptor table:

```c
// kernel/fs/file.c

#define MAX_GLOBAL_FDS  64

struct file_desc {
    int in_use;                 // Is this FD allocated?
    int brfs_fd;                // Underlying BRFS file handle
    unsigned int flags;         // Open flags
    unsigned int cursor;        // Current position
    int is_dir;                 // Is this a directory?
};

static struct file_desc fd_table[MAX_GLOBAL_FDS];

// Initialize file system
int fs_init(void) {
    // Clear FD table
    for (int i = 0; i < MAX_GLOBAL_FDS; i++) {
        fd_table[i].in_use = 0;
    }
    
    // Initialize BRFS
    int result = brfs_init(SPI_FLASH_1);  // Use second SPI Flash
    if (result != BRFS_OK) {
        return result;
    }
    
    // Mount filesystem
    return brfs_mount();
}

// Allocate a new FD
static int fd_alloc(void) {
    for (int i = 3; i < MAX_GLOBAL_FDS; i++) {  // 0,1,2 reserved for stdio
        if (!fd_table[i].in_use) {
            fd_table[i].in_use = 1;
            return i;
        }
    }
    return -EMFILE;
}

// Free an FD
static void fd_free(int fd) {
    if (fd >= 3 && fd < MAX_GLOBAL_FDS) {
        fd_table[fd].in_use = 0;
    }
}
```

### 3.3 File Operations Implementation

```c
// kernel/fs/file.c (continued)

int fs_open(const char* path, int flags) {
    // Allocate FD
    int fd = fd_alloc();
    if (fd < 0) return fd;
    
    // Convert path to absolute if needed
    char abs_path[BRFS_MAX_PATH_LENGTH];
    if (path[0] != '/') {
        fs_getcwd(abs_path, BRFS_MAX_PATH_LENGTH);
        strcat(abs_path, "/");
        strcat(abs_path, path);
    } else {
        strcpy(abs_path, path);
    }
    
    // Handle O_CREATE
    if (flags & O_CREATE) {
        // Check if file exists
        struct brfs_dir_entry* entry = brfs_stat_entry(abs_path);
        if (entry == NULL) {
            // Create new file
            char dirname[BRFS_MAX_PATH_LENGTH];
            char* basename = path_basename(abs_path);
            path_dirname(abs_path, dirname);
            
            int result = brfs_create_file(dirname, basename);
            if (result != BRFS_OK) {
                fd_free(fd);
                return result;
            }
        }
    }
    
    // Open in BRFS
    int brfs_fd = brfs_open(abs_path);
    if (brfs_fd < 0) {
        fd_free(fd);
        return brfs_fd;
    }
    
    fd_table[fd].brfs_fd = brfs_fd;
    fd_table[fd].flags = flags;
    fd_table[fd].cursor = 0;
    fd_table[fd].is_dir = 0;
    
    // Handle O_TRUNC
    if (flags & O_TRUNC) {
        brfs_truncate(brfs_fd, 0);
    }
    
    // Handle O_APPEND
    if (flags & O_APPEND) {
        int size = brfs_get_size(brfs_fd);
        fd_table[fd].cursor = size;
        brfs_seek(brfs_fd, size);
    }
    
    return fd;
}

int fs_close(int fd) {
    if (fd < 3 || fd >= MAX_GLOBAL_FDS || !fd_table[fd].in_use) {
        return -EBADF;
    }
    
    int result = brfs_close(fd_table[fd].brfs_fd);
    fd_free(fd);
    return result;
}

int fs_read(int fd, void* buf, unsigned int count) {
    if (fd < 3 || fd >= MAX_GLOBAL_FDS || !fd_table[fd].in_use) {
        return -EBADF;
    }
    
    if (!(fd_table[fd].flags & O_READ)) {
        return -EACCES;
    }
    
    int bytes_read = brfs_read(fd_table[fd].brfs_fd, buf, count);
    if (bytes_read > 0) {
        fd_table[fd].cursor += bytes_read;
    }
    return bytes_read;
}

int fs_write(int fd, const void* buf, unsigned int count) {
    if (fd < 3 || fd >= MAX_GLOBAL_FDS || !fd_table[fd].in_use) {
        return -EBADF;
    }
    
    if (!(fd_table[fd].flags & O_WRITE)) {
        return -EACCES;
    }
    
    int bytes_written = brfs_write(fd_table[fd].brfs_fd, buf, count);
    if (bytes_written > 0) {
        fd_table[fd].cursor += bytes_written;
    }
    return bytes_written;
}

int fs_seek(int fd, int offset, int whence) {
    if (fd < 3 || fd >= MAX_GLOBAL_FDS || !fd_table[fd].in_use) {
        return -EBADF;
    }
    
    int new_pos;
    int size = brfs_get_size(fd_table[fd].brfs_fd);
    
    switch (whence) {
        case SEEK_SET:
            new_pos = offset;
            break;
        case SEEK_CUR:
            new_pos = fd_table[fd].cursor + offset;
            break;
        case SEEK_END:
            new_pos = size + offset;
            break;
        default:
            return -EINVAL;
    }
    
    if (new_pos < 0 || new_pos > size) {
        return -EINVAL;
    }
    
    fd_table[fd].cursor = new_pos;
    return brfs_seek(fd_table[fd].brfs_fd, new_pos);
}
```

---

## 4. Loading Filesystem from SPI Flash

### 4.1 Boot-Time Loading

From stakeholder requirements:
> "I want the OS to load the BRFS filesystem from one of the SPI Flash chips into the last 32 MiB of RAM"

```c
// kernel/fs/init.c

#include "brfs.h"
#include "kernel/io/spi_flash.h"

#define BRFS_FLASH_ID       SPI_FLASH_1     // Second SPI Flash chip
#define BRFS_RAM_START      0x1000000       // 16 MiW (64 MiB) offset
#define BRFS_RAM_SIZE       0x0800000       // 8 MiW (32 MiB)

int fs_load_from_flash(void) {
    term_puts("Loading filesystem from SPI Flash...\n");
    
    // Initialize SPI Flash
    spi_flash_init(BRFS_FLASH_ID);
    
    // Initialize BRFS with RAM cache location
    brfs_set_cache_addr((unsigned int*)BRFS_RAM_START, BRFS_RAM_SIZE);
    
    // Mount from flash (reads superblock, FAT, and data)
    int result = brfs_mount_from_flash(BRFS_FLASH_ID);
    
    if (result == BRFS_OK) {
        term_puts("Filesystem mounted successfully\n");
        
        // Print filesystem info
        struct brfs_superblock* sb = brfs_get_superblock();
        term_puts("  Label: ");
        term_puts((char*)sb->label);
        term_puts("\n  Blocks: ");
        term_putint(sb->total_blocks);
        term_puts("\n  Block size: ");
        term_putint(sb->words_per_block);
        term_puts(" words\n");
    } else {
        term_puts("Failed to mount filesystem!\n");
        term_puts("Run 'format' to create new filesystem.\n");
    }
    
    return result;
}
```

### 4.2 Sync to Flash

```c
// Sync modified blocks to SPI Flash
int fs_sync(void) {
    term_puts("Syncing filesystem to flash...\n");
    
    int result = brfs_sync_to_flash(BRFS_FLASH_ID);
    
    if (result == BRFS_OK) {
        term_puts("Sync complete\n");
    } else {
        term_puts("Sync failed!\n");
    }
    
    return result;
}
```

### 4.3 Dirty Block Tracking

The existing BRFS implementation tracks dirty blocks:

```c
// From brfs.h
unsigned int dirty_blocks[(BRFS_MAX_BLOCKS + 31) / 32];

// Mark block as dirty
void brfs_mark_dirty(unsigned int block_idx) {
    dirty_blocks[block_idx >> 5] |= (1 << (block_idx & 31));
}

// Check if block is dirty
int brfs_is_dirty(unsigned int block_idx) {
    return (dirty_blocks[block_idx >> 5] >> (block_idx & 31)) & 1;
}

// Sync only dirty blocks to flash
int brfs_sync_dirty(void) {
    struct brfs_superblock* sb = brfs_get_superblock();
    
    for (unsigned int i = 0; i < sb->total_blocks; i++) {
        if (brfs_is_dirty(i)) {
            // Write block to flash
            unsigned int* block_addr = brfs_get_block_addr(i);
            unsigned int flash_addr = BRFS_FLASH_DATA_ADDR + 
                                     (i * sb->words_per_block * 4);
            
            spi_flash_write_block(flash_addr, block_addr, 
                                 sb->words_per_block * 4);
            
            // Clear dirty flag
            dirty_blocks[i >> 5] &= ~(1 << (i & 31));
        }
    }
    
    return BRFS_OK;
}
```

---

## 5. Directory Operations

### 5.1 Working Directory

The kernel maintains a current working directory per process:

```c
// In struct process
char cwd[BRFS_MAX_PATH_LENGTH];  // Current working directory

// Initialize to root
strcpy(proc->cwd, "/");
```

```c
// kernel/fs/dir.c

int fs_getcwd(char* buf, unsigned int size) {
    struct process* proc = proc_get_current();
    if (proc == NULL) {
        // Kernel context, use root
        strncpy(buf, "/", size);
        return 0;
    }
    
    strncpy(buf, proc->cwd, size);
    return 0;
}

int fs_chdir(const char* path) {
    struct process* proc = proc_get_current();
    
    // Resolve to absolute path
    char abs_path[BRFS_MAX_PATH_LENGTH];
    fs_resolve_path(path, abs_path);
    
    // Verify it's a directory
    struct brfs_dir_entry* entry = brfs_stat_entry(abs_path);
    if (entry == NULL) {
        return -ENOENT;
    }
    if (!(entry->flags & BRFS_FLAG_DIRECTORY)) {
        return -ENOTDIR;
    }
    
    // Update CWD
    strcpy(proc->cwd, abs_path);
    return 0;
}
```

### 5.2 Directory Listing

```c
// Open directory for reading
int fs_opendir(const char* path) {
    // Resolve path
    char abs_path[BRFS_MAX_PATH_LENGTH];
    fs_resolve_path(path, abs_path);
    
    // Get directory FAT index
    int fat_idx = brfs_get_dir_fat_idx(abs_path);
    if (fat_idx < 0) {
        return -ENOENT;
    }
    
    // Allocate FD
    int fd = fd_alloc();
    if (fd < 0) return fd;
    
    fd_table[fd].brfs_fd = fat_idx;  // Store FAT index
    fd_table[fd].flags = O_READ;
    fd_table[fd].cursor = 0;         // Entry index
    fd_table[fd].is_dir = 1;
    
    return fd;
}

int fs_readdir(int fd, struct dirent* entry) {
    if (fd < 3 || fd >= MAX_GLOBAL_FDS || !fd_table[fd].in_use) {
        return -EBADF;
    }
    
    if (!fd_table[fd].is_dir) {
        return -ENOTDIR;
    }
    
    int fat_idx = fd_table[fd].brfs_fd;
    int entry_idx = fd_table[fd].cursor;
    
    // Read directory entry from BRFS
    struct brfs_dir_entry* brfs_entry = 
        brfs_read_dir_entry(fat_idx, entry_idx);
    
    if (brfs_entry == NULL || brfs_entry->filename[0] == 0) {
        return 0;  // No more entries
    }
    
    // Decompress filename
    brfs_decompress_name(brfs_entry->filename, entry->name);
    entry->size = brfs_entry->filesize;
    entry->flags = brfs_entry->flags;
    entry->type = (brfs_entry->flags & BRFS_FLAG_DIRECTORY) ? FT_DIR : FT_REGULAR;
    
    fd_table[fd].cursor++;
    return 1;  // Entry read successfully
}

int fs_closedir(int fd) {
    return fs_close(fd);
}
```

---

## 6. Path Resolution

### 6.1 Path Utilities

```c
// kernel/fs/path.c

// Resolve relative path to absolute
int fs_resolve_path(const char* path, char* abs_path) {
    if (path[0] == '/') {
        // Already absolute
        strcpy(abs_path, path);
    } else {
        // Prepend CWD
        fs_getcwd(abs_path, BRFS_MAX_PATH_LENGTH);
        if (abs_path[strlen(abs_path) - 1] != '/') {
            strcat(abs_path, "/");
        }
        strcat(abs_path, path);
    }
    
    // Normalize path (handle . and ..)
    return fs_normalize_path(abs_path);
}

// Normalize path, handling . and ..
int fs_normalize_path(char* path) {
    char result[BRFS_MAX_PATH_LENGTH];
    char* components[32];
    int num_components = 0;
    
    // Start with root
    result[0] = '/';
    result[1] = '\0';
    
    // Tokenize path
    char* token = strtok(path + 1, "/");
    while (token != NULL) {
        if (strcmp(token, ".") == 0) {
            // Skip current directory
        } else if (strcmp(token, "..") == 0) {
            // Go up one level
            if (num_components > 0) {
                num_components--;
            }
        } else {
            // Add component
            if (num_components < 32) {
                components[num_components++] = token;
            }
        }
        token = strtok(NULL, "/");
    }
    
    // Rebuild path
    for (int i = 0; i < num_components; i++) {
        if (i > 0) strcat(result, "/");
        strcat(result, components[i]);
    }
    
    // Ensure root if empty
    if (result[0] == '\0') {
        strcpy(result, "/");
    }
    
    strcpy(path, result);
    return 0;
}

// Get directory name from path
void path_dirname(const char* path, char* dirname) {
    strcpy(dirname, path);
    char* last_slash = strrchr(dirname, '/');
    if (last_slash != NULL) {
        if (last_slash == dirname) {
            dirname[1] = '\0';  // Root directory
        } else {
            *last_slash = '\0';
        }
    } else {
        strcpy(dirname, ".");
    }
}

// Get base name from path
char* path_basename(const char* path) {
    char* last_slash = strrchr(path, '/');
    if (last_slash != NULL) {
        return last_slash + 1;
    }
    return (char*)path;
}
```

---

## 7. File System Commands (Shell Built-ins)

### 7.1 Built-in Commands

The shell should have built-in commands for filesystem operations:

```c
// shell/commands.c

void cmd_ls(int argc, char** argv) {
    const char* path = (argc > 1) ? argv[1] : ".";
    
    int fd = fs_opendir(path);
    if (fd < 0) {
        term_puts("Cannot open directory\n");
        return;
    }
    
    struct dirent entry;
    while (fs_readdir(fd, &entry) > 0) {
        // Skip . and ..
        if (entry.name[0] == '.') continue;
        
        // Print type indicator
        if (entry.type == FT_DIR) {
            term_putchar('[');
        }
        
        term_puts(entry.name);
        
        if (entry.type == FT_DIR) {
            term_putchar(']');
        } else {
            term_puts("  ");
            term_putint(entry.size);
        }
        term_putchar('\n');
    }
    
    fs_closedir(fd);
}

void cmd_cd(int argc, char** argv) {
    const char* path = (argc > 1) ? argv[1] : "/";
    
    int result = fs_chdir(path);
    if (result < 0) {
        term_puts("Cannot change directory\n");
    }
}

void cmd_pwd(int argc, char** argv) {
    char buf[BRFS_MAX_PATH_LENGTH];
    fs_getcwd(buf, sizeof(buf));
    term_puts(buf);
    term_putchar('\n');
}

void cmd_mkdir(int argc, char** argv) {
    if (argc < 2) {
        term_puts("Usage: mkdir <dir>\n");
        return;
    }
    
    int result = fs_mkdir(argv[1]);
    if (result < 0) {
        term_puts("Cannot create directory\n");
    }
}

void cmd_rm(int argc, char** argv) {
    if (argc < 2) {
        term_puts("Usage: rm <file>\n");
        return;
    }
    
    int result = fs_unlink(argv[1]);
    if (result < 0) {
        term_puts("Cannot remove file\n");
    }
}

void cmd_cat(int argc, char** argv) {
    if (argc < 2) {
        term_puts("Usage: cat <file>\n");
        return;
    }
    
    int fd = fs_open(argv[1], O_READ);
    if (fd < 0) {
        term_puts("Cannot open file\n");
        return;
    }
    
    char buf[64];
    int n;
    while ((n = fs_read(fd, buf, 64)) > 0) {
        for (int i = 0; i < n; i++) {
            term_putchar(buf[i]);
        }
    }
    
    fs_close(fd);
}

void cmd_sync(int argc, char** argv) {
    fs_sync();
}

void cmd_df(int argc, char** argv) {
    // Display filesystem usage
    struct brfs_superblock* sb = brfs_get_superblock();
    unsigned int used = brfs_count_used_blocks();
    unsigned int total = sb->total_blocks;
    unsigned int free = total - used;
    
    term_puts("Filesystem: ");
    term_puts((char*)sb->label);
    term_puts("\n");
    
    term_puts("Used: ");
    term_putint(used);
    term_puts(" blocks (");
    term_putint((used * 100) / total);
    term_puts("%)\n");
    
    term_puts("Free: ");
    term_putint(free);
    term_puts(" blocks\n");
    
    term_puts("Block size: ");
    term_putint(sb->words_per_block);
    term_puts(" words\n");
}
```

---

## 8. File System for Network Loading

### 8.1 Receiving Files via Network

From stakeholder requirements:
> "I want to be able to send program binaries to BDOS to store them in BRFS"

The network subsystem can store received files:

```c
// kernel/net/netloader.c

int netloader_save_file(const char* filename, void* data, unsigned int size) {
    // Determine path (default to /bin for programs)
    char path[BRFS_MAX_PATH_LENGTH];
    strcpy(path, "/bin/");
    strcat(path, filename);
    
    // Delete if exists
    fs_unlink(path);
    
    // Create and write file
    int fd = fs_open(path, O_WRITE | O_CREATE);
    if (fd < 0) {
        return fd;
    }
    
    int written = fs_write(fd, data, size);
    fs_close(fd);
    
    if (written != size) {
        return -EIO;
    }
    
    // Sync to flash
    fs_sync();
    
    term_puts("Saved: ");
    term_puts(path);
    term_puts(" (");
    term_putint(size);
    term_puts(" words)\n");
    
    return 0;
}
```

---

## 9. Design Alternatives

### Alternative A: Lazy Loading from Flash

Instead of loading entire filesystem to RAM:

**Concept**: Only load blocks as needed, cache frequently used blocks.

**Pros:**
- Less RAM usage
- Faster boot

**Cons:**
- Slower file access
- More complex implementation
- SPI Flash read latency

**Verdict**: For 32 MiB filesystem fitting in RAM, full load is simpler and faster. Consider for larger filesystems.

### Alternative B: Multiple Filesystems

Support mounting multiple filesystems:

**Concept**: Mount second filesystem at specific path (e.g., /mnt/sd)

**Pros:**
- SD card support
- More flexibility

**Cons:**
- More complex VFS layer
- Not immediately needed

**Verdict**: Design VFS to support this, implement later.

### Alternative C: In-Memory Filesystem (tmpfs)

Add a RAM-only filesystem for temporary files:

**Concept**: /tmp mounted as pure RAM filesystem, not persisted

**Pros:**
- Fast temporary storage
- No flash wear

**Cons:**
- Lost on reboot
- Additional code

**Verdict**: Good idea for future, especially for piping.

---

## 10. Implementation Checklist

- [ ] Implement `fs_init()` with BRFS mount
- [ ] Implement file descriptor table
- [ ] Implement `fs_open()`, `fs_close()`, `fs_read()`, `fs_write()`
- [ ] Implement `fs_seek()`, `fs_tell()`
- [ ] Implement `fs_stat()` for file info
- [ ] Implement directory operations (opendir, readdir, closedir)
- [ ] Implement `fs_getcwd()`, `fs_chdir()`
- [ ] Implement `fs_mkdir()`, `fs_rmdir()`, `fs_unlink()`
- [ ] Implement path resolution and normalization
- [ ] Implement `fs_sync()` for flash persistence
- [ ] Add shell commands (ls, cd, pwd, mkdir, rm, cat, df, sync)
- [ ] Test with sample files
- [ ] Test network file upload

---

## 11. Summary

| Feature | Status | Notes |
|---------|--------|-------|
| BRFS Integration | Via existing lib | Full RAM cache |
| File Descriptors | Global table | 64 max |
| Syscall Interface | fs_* wrappers | POSIX-like |
| Working Directory | Per-process | CWD support |
| Flash Persistence | On sync | Dirty block tracking |
| Shell Commands | Built-in | ls, cd, pwd, etc. |
