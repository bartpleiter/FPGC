#ifndef FPGC_BRFS_H
#define FPGC_BRFS_H

#include <stddef.h>
#include "brfs_storage.h"

#ifndef BRFS_PROGRESS_CALLBACK_T_DEFINED
#define BRFS_PROGRESS_CALLBACK_T_DEFINED
typedef void (*brfs_progress_callback_t)(const char *phase, unsigned int current, unsigned int total);
#endif

#include "brfs_cache.h"

/* ---- Configuration Constants ---- */
#define BRFS_VERSION 1

#define BRFS_MAX_PATH_LENGTH    127
#define BRFS_MAX_FILENAME_LENGTH 16
#define BRFS_MAX_OPEN_FILES     16
#define BRFS_MAX_BLOCKS         65536

/* SPI Flash layout addresses (in bytes) */
#define BRFS_FLASH_SUPERBLOCK_ADDR 0x00000
#define BRFS_FLASH_FAT_ADDR        0x01000
#define BRFS_FLASH_DATA_ADDR       0x10000

/* SPI Flash geometry */
#define BRFS_FLASH_SECTOR_SIZE      4096
#define BRFS_FLASH_PAGE_SIZE        256
#define BRFS_FLASH_WORDS_PER_SECTOR (BRFS_FLASH_SECTOR_SIZE / 4)
#define BRFS_FLASH_WORDS_PER_PAGE   (BRFS_FLASH_PAGE_SIZE / 4)

/* Structure sizes in words */
#define BRFS_SUPERBLOCK_SIZE 16
#define BRFS_DIR_ENTRY_SIZE  8

/* FAT special values */
#define BRFS_FAT_FREE 0
#define BRFS_FAT_EOF  ((unsigned int)-1)

/* File/Directory flags */
#define BRFS_FLAG_DIRECTORY 0x01
#define BRFS_FLAG_HIDDEN    0x02

/* ---- Error Codes ---- */
#define BRFS_OK                      0
#define BRFS_ERR_INVALID_PARAM      -1
#define BRFS_ERR_NOT_FOUND          -2
#define BRFS_ERR_EXISTS             -3
#define BRFS_ERR_NO_SPACE           -4
#define BRFS_ERR_NO_ENTRY           -5
#define BRFS_ERR_NOT_EMPTY          -6
#define BRFS_ERR_IS_OPEN            -7
#define BRFS_ERR_NOT_OPEN           -8
#define BRFS_ERR_TOO_MANY_OPEN      -9
#define BRFS_ERR_IS_DIRECTORY       -10
#define BRFS_ERR_NOT_DIRECTORY      -11
#define BRFS_ERR_PATH_TOO_LONG      -12
#define BRFS_ERR_NAME_TOO_LONG      -13
#define BRFS_ERR_INVALID_SUPERBLOCK -14
#define BRFS_ERR_FLASH_ERROR        -15
#define BRFS_ERR_SEEK_ERROR         -16
#define BRFS_ERR_READ_ERROR         -17
#define BRFS_ERR_WRITE_ERROR        -18
#define BRFS_ERR_NOT_INITIALIZED    -19

/* ---- Data Structures ---- */

struct brfs_superblock
{
  unsigned int total_blocks;
  unsigned int words_per_block;
  unsigned int label[10];
  unsigned int brfs_version;
  unsigned int reserved[3];
};

struct brfs_dir_entry
{
  unsigned int filename[4];
  unsigned int modify_date;
  unsigned int flags;
  unsigned int fat_idx;
  unsigned int filesize;
};

struct brfs_file
{
  unsigned int fat_idx;
  unsigned int cursor;
  struct brfs_dir_entry *dir_entry;
};

struct brfs_state
{
  unsigned int *cache;
  unsigned int cache_size;
  unsigned int initialized;
  brfs_cache_t cache_state;
  struct brfs_file open_files[BRFS_MAX_OPEN_FILES];
};

/* ---- Initialization ---- */
int  brfs_init(brfs_storage_t *storage, unsigned int *cache_addr, unsigned int cache_size);
void brfs_set_progress_callback(brfs_progress_callback_t callback);
int  brfs_format(unsigned int total_blocks, unsigned int words_per_block,
                 const char *label, int full_format);
int  brfs_mount(void);
int  brfs_unmount(void);
int  brfs_sync(void);

/* ---- File Operations ---- */
int brfs_create_file(const char *path);
int brfs_open(const char *path);
int brfs_close(int fd);
void brfs_close_all(void);
int brfs_read(int fd, unsigned int *buffer, unsigned int length);
int brfs_write(int fd, const unsigned int *buffer, unsigned int length);
int brfs_seek(int fd, unsigned int offset);
int brfs_tell(int fd);
int brfs_file_size(int fd);

/* ---- Directory Operations ---- */
int brfs_create_dir(const char *path);
int brfs_read_dir(const char *path, struct brfs_dir_entry *buffer, unsigned int max_entries);

/* ---- File/Directory Management ---- */
int brfs_delete(const char *path);
int brfs_stat(const char *path, struct brfs_dir_entry *entry);
int brfs_exists(const char *path);
int brfs_is_dir(const char *path);

/* ---- Utility ---- */
const char *brfs_strerror(int error_code);
int brfs_statfs(unsigned int *total_blocks, unsigned int *free_blocks,
                unsigned int *block_size);
int brfs_get_label(char *label_buffer, unsigned int buffer_size);

/* ---- Internal Helpers (exposed for testing) ---- */
void brfs_compress_string(unsigned int *dest, const char *src);
void brfs_decompress_string(char *dest, const unsigned int *src, unsigned int src_words);
int  brfs_parse_path(const char *path, char *dir_path, char *filename,
                     unsigned int dir_path_size);

#endif /* FPGC_BRFS_H */
