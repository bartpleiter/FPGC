#include "brfs.h"
#include <string.h>

/* ---- Global instance removed; callers own their brfs_state. ---- */

static void brfs_report_progress(struct brfs_state *fs, const char *phase, unsigned int current, unsigned int total)
{
  if (fs->progress_callback != NULL)
  {
    fs->progress_callback(phase, current, total);
  }
}

/* ---- Internal Helper Functions - Forward Declarations ---- */
static unsigned int *brfs_get_superblock(struct brfs_state *fs);
static unsigned int *brfs_get_fat(struct brfs_state *fs);
static unsigned int *brfs_get_data_block(struct brfs_state *fs, unsigned int block_idx);
static int brfs_find_free_block(struct brfs_state *fs);
static int brfs_find_free_dir_entry(struct brfs_state *fs, unsigned int *dir_block);
static int brfs_get_dir_fat_idx(struct brfs_state *fs, const char *dir_path);
static int brfs_find_in_directory(struct brfs_state *fs, unsigned int dir_fat_idx,
                                  const char *name,
                                  struct brfs_dir_entry **entry_out,
                                  unsigned int *entry_idx_out);
static void brfs_mark_block_dirty(struct brfs_state *fs, unsigned int block_idx);
static int brfs_get_fat_idx_at_offset(struct brfs_state *fs, unsigned int start_fat_idx,
                                      unsigned int offset);
static void brfs_init_directory_block(struct brfs_state *fs, unsigned int *block_addr,
                                      unsigned int dir_fat_idx,
                                      unsigned int parent_fat_idx);
static void brfs_create_dir_entry(struct brfs_dir_entry *entry, const char *filename,
                                  unsigned int fat_idx, unsigned int filesize,
                                  unsigned int flags);

/* ---- String compression/decompression ---- */

void brfs_compress_string(unsigned int *dest, const char *src)
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
    word |= c << (24 - (char_idx & 3) * 8);

    if (c == 0)
    {
      dest[word_idx] = word;
      word_idx++;
      while (word_idx < 4)
      {
        dest[word_idx] = 0;
        word_idx++;
      }
      break;
    }

    char_idx++;

    if ((char_idx & 3) == 0)
    {
      dest[word_idx] = word;
      word_idx++;
      word = 0;

      if (word_idx >= 4)
      {
        break;
      }
    }
  }
}

void brfs_decompress_string(char *dest, const unsigned int *src, unsigned int src_words)
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
    if (c == 0)
      return;

    c = (word >> 16) & 0xFF;
    dest[char_idx++] = c;
    if (c == 0)
      return;

    c = (word >> 8) & 0xFF;
    dest[char_idx++] = c;
    if (c == 0)
      return;

    c = word & 0xFF;
    dest[char_idx++] = c;
    if (c == 0)
      return;
  }

  dest[char_idx] = '\0';
}

/* ---- Path Parsing ---- */

int brfs_parse_path(const char *path, char *dir_path, char *filename,
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
    dir_path[0] = '/';
    dir_path[1] = '\0';
    strcpy(filename, path);
  }
  else if (last_slash == 0)
  {
    dir_path[0] = '/';
    dir_path[1] = '\0';
    strcpy(filename, path + 1);
  }
  else
  {
    if ((unsigned int)last_slash >= dir_path_size)
    {
      return BRFS_ERR_PATH_TOO_LONG;
    }

    for (i = 0; i < last_slash; i++)
    {
      dir_path[i] = path[i];
    }
    dir_path[last_slash] = '\0';

    strcpy(filename, path + last_slash + 1);
  }

  if (strlen(filename) == 0 || strlen(filename) > BRFS_MAX_FILENAME_LENGTH)
  {
    return BRFS_ERR_NAME_TOO_LONG;
  }

  return BRFS_OK;
}

/* ---- Internal Cache Access Functions ---- */

static unsigned int *brfs_get_superblock(struct brfs_state *fs)
{
  return brfs_cache_superblock(&fs->cache_state);
}

static unsigned int *brfs_get_fat(struct brfs_state *fs)
{
  return brfs_cache_fat(&fs->cache_state);
}

static unsigned int *brfs_get_data_block(struct brfs_state *fs, unsigned int block_idx)
{
  return brfs_cache_data(&fs->cache_state, block_idx);
}

/* ---- Block Allocation Functions ---- */

static int brfs_find_free_block(struct brfs_state *fs)
{
  struct brfs_superblock *sb;
  unsigned int *fat;
  unsigned int i;

  sb = (struct brfs_superblock *)brfs_get_superblock(fs);
  fat = brfs_get_fat(fs);

  for (i = 0; i < sb->total_blocks; i++)
  {
    if (fat[i] == BRFS_FAT_FREE)
    {
      return (int)i;
    }
  }

  return BRFS_ERR_NO_SPACE;
}

static int brfs_find_free_dir_entry(struct brfs_state *fs, unsigned int *dir_block)
{
  struct brfs_superblock *sb;
  unsigned int max_entries;
  unsigned int i;
  struct brfs_dir_entry *entry;

  sb = (struct brfs_superblock *)brfs_get_superblock(fs);
  max_entries = sb->words_per_block / BRFS_DIR_ENTRY_SIZE;

  for (i = 0; i < max_entries; i++)
  {
    entry = (struct brfs_dir_entry *)(dir_block + (i * BRFS_DIR_ENTRY_SIZE));
    if (entry->filename[0] == 0)
    {
      return (int)i;
    }
  }

  return BRFS_ERR_NO_ENTRY;
}

static void brfs_mark_block_dirty(struct brfs_state *fs, unsigned int block_idx)
{
  brfs_cache_mark_dirty(&fs->cache_state, block_idx);
}

/* ---- FAT Chain Navigation ----
 * v2: byte_offset selects a block via byte_offset / (words_per_block*4). */

static int brfs_get_fat_idx_at_offset(struct brfs_state *fs, unsigned int start_fat_idx, unsigned int byte_offset)
{
  struct brfs_superblock *sb;
  unsigned int *fat;
  unsigned int current_idx;
  unsigned int blocks_to_skip;
  unsigned int bytes_per_block;

  sb = (struct brfs_superblock *)brfs_get_superblock(fs);
  fat = brfs_get_fat(fs);

  current_idx = start_fat_idx;
  bytes_per_block = sb->words_per_block * 4u;
  blocks_to_skip = byte_offset / bytes_per_block;

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

/* ---- Directory Navigation and Lookup ---- */

static int brfs_get_dir_fat_idx(struct brfs_state *fs, const char *dir_path)
{
  struct brfs_superblock *sb;
  unsigned int current_fat_idx;
  char path_copy[BRFS_MAX_PATH_LENGTH + 1];
  char token[BRFS_MAX_FILENAME_LENGTH + 1];
  int path_idx;
  int token_idx;
  int len;
  struct brfs_dir_entry *found_entry;
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

  if (len == 0 || (len == 1 && dir_path[0] == '/'))
  {
    return 0;
  }

  sb = (struct brfs_superblock *)brfs_get_superblock(fs);
  current_fat_idx = 0;

  strcpy(path_copy, dir_path);

  path_idx = 0;
  if (path_copy[0] == '/')
  {
    path_idx = 1;
  }

  while (path_copy[path_idx] != '\0')
  {
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

    if (path_copy[path_idx] == '/')
    {
      path_idx++;
    }

    if (token_idx == 0)
    {
      continue;
    }

    result = brfs_find_in_directory(fs, current_fat_idx, token, &found_entry, NULL);
    if (result != BRFS_OK)
    {
      return result;
    }

    if ((found_entry->flags & BRFS_FLAG_DIRECTORY) == 0)
    {
      return BRFS_ERR_NOT_DIRECTORY;
    }

    current_fat_idx = found_entry->fat_idx;
  }

  return (int)current_fat_idx;
}

static int brfs_find_in_directory(struct brfs_state *fs, unsigned int dir_fat_idx, const char *name,
                                  struct brfs_dir_entry **entry_out,
                                  unsigned int *entry_idx_out)
{
  struct brfs_superblock *sb;
  unsigned int *dir_block;
  unsigned int max_entries;
  unsigned int i;
  struct brfs_dir_entry *entry;
  char entry_name[BRFS_MAX_FILENAME_LENGTH + 1];

  sb = (struct brfs_superblock *)brfs_get_superblock(fs);
  dir_block = brfs_get_data_block(fs, dir_fat_idx);
  max_entries = sb->words_per_block / BRFS_DIR_ENTRY_SIZE;

  for (i = 0; i < max_entries; i++)
  {
    entry = (struct brfs_dir_entry *)(dir_block + (i * BRFS_DIR_ENTRY_SIZE));

    if (entry->filename[0] != 0)
    {
      brfs_decompress_string(entry_name, entry->filename, 4);

      if (strcmp(entry_name, name) == 0)
      {
        if (entry_out != NULL)
        {
          *entry_out = entry;
        }
        if (entry_idx_out != NULL)
        {
          *entry_idx_out = i;
        }
        return BRFS_OK;
      }
    }
  }

  return BRFS_ERR_NOT_FOUND;
}

/* ---- Directory Entry Creation ---- */

static void brfs_create_dir_entry(struct brfs_dir_entry *entry, const char *filename,
                                  unsigned int fat_idx, unsigned int filesize,
                                  unsigned int flags)
{
  memset(entry, 0, sizeof(struct brfs_dir_entry));
  brfs_compress_string(entry->filename, filename);
  entry->fat_idx = fat_idx;
  entry->filesize = filesize;
  entry->flags = flags;
  entry->modify_date = 0;
}

static void brfs_init_directory_block(struct brfs_state *fs, unsigned int *block_addr, unsigned int dir_fat_idx,
                                      unsigned int parent_fat_idx)
{
  struct brfs_superblock *sb;
  struct brfs_dir_entry entry;
  unsigned int max_entries;

  sb = (struct brfs_superblock *)brfs_get_superblock(fs);
  max_entries = sb->words_per_block / BRFS_DIR_ENTRY_SIZE;

  memset(block_addr, 0, sb->words_per_block * sizeof(unsigned int));

  brfs_create_dir_entry(&entry, ".", dir_fat_idx,
                        max_entries * BRFS_DIR_ENTRY_SIZE * 4u, BRFS_FLAG_DIRECTORY);
  memcpy(block_addr, &entry, sizeof(entry));

  brfs_create_dir_entry(&entry, "..", parent_fat_idx,
                        max_entries * BRFS_DIR_ENTRY_SIZE * 4u, BRFS_FLAG_DIRECTORY);
  memcpy(block_addr + BRFS_DIR_ENTRY_SIZE, &entry, sizeof(entry));
}

/* ---- Initialization Functions ---- */

int brfs_init(struct brfs_state *fs, brfs_storage_t *storage, unsigned int *cache_addr, unsigned int cache_size)
{
  unsigned int i;

  fs->cache = cache_addr;
  fs->cache_size = cache_size;
  fs->initialized = 0;
  fs->progress_callback = NULL;

  brfs_cache_init(&fs->cache_state, storage, cache_addr, cache_size);
  brfs_cache_set_layout(&fs->cache_state,
                        BRFS_FLASH_SUPERBLOCK_ADDR,
                        BRFS_FLASH_FAT_ADDR,
                        BRFS_FLASH_DATA_ADDR);

  for (i = 0; i < BRFS_MAX_OPEN_FILES; i++)
  {
    fs->open_files[i].fat_idx = 0;
    fs->open_files[i].cursor = 0;
    fs->open_files[i].in_use = 0;
  }

  return BRFS_OK;
}

void brfs_set_progress_callback(struct brfs_state *fs, brfs_progress_callback_t callback)
{
  fs->progress_callback = callback;
}

int brfs_format(struct brfs_state *fs, unsigned int total_blocks, unsigned int words_per_block,
                const char *label, int full_format)
{
  struct brfs_superblock *sb;
  unsigned int *fat;
  unsigned int *root_block;
  unsigned int i;
  unsigned int data_size;

  if (total_blocks == 0 || total_blocks > BRFS_MAX_BLOCKS)
  {
    return BRFS_ERR_INVALID_PARAM;
  }

  if (words_per_block == 0 || words_per_block > 2048)
  {
    return BRFS_ERR_INVALID_PARAM;
  }

  if ((total_blocks & 63) != 0)
  {
    return BRFS_ERR_INVALID_PARAM;
  }

  if ((words_per_block & 63) != 0)
  {
    return BRFS_ERR_INVALID_PARAM;
  }

  data_size = BRFS_SUPERBLOCK_SIZE + total_blocks + (total_blocks * words_per_block);
  if (data_size > fs->cache_size)
  {
    unsigned int min_lru = BRFS_SUPERBLOCK_SIZE +
                           total_blocks * 2u +
                           words_per_block + 4u;
    if (min_lru > fs->cache_size)
    {
      return BRFS_ERR_NO_SPACE;
    }
  }

  sb = (struct brfs_superblock *)brfs_get_superblock(fs);
  memset(sb, 0, sizeof(struct brfs_superblock));

  sb->magic = BRFS_MAGIC;
  sb->total_blocks = total_blocks;
  sb->words_per_block = words_per_block;
  sb->brfs_version = BRFS_VERSION;

  /* Cache geometry must be known before any brfs_get_fat/data call below. */
  brfs_cache_configure(&fs->cache_state, total_blocks, words_per_block);

  if (label != NULL)
  {
    for (i = 0; i < 10 && label[i] != '\0'; i++)
    {
      sb->label[i] = (unsigned int)label[i];
    }
  }

  fat = brfs_get_fat(fs);
  memset(fat, 0, total_blocks * sizeof(unsigned int));

  if (full_format)
  {
    for (i = 0; i < (int)total_blocks; i++)
    {
      memset(brfs_get_data_block(fs, i), 0, words_per_block * sizeof(unsigned int));
      brfs_report_progress(fs, "format-zero", i + 1, total_blocks);
    }
  }

  root_block = brfs_get_data_block(fs, 0);
  brfs_init_directory_block(fs, root_block, 0, 0);

  fat[0] = BRFS_FAT_EOF;

  for (i = 0; i < total_blocks; i++)
  {
    brfs_mark_block_dirty(fs, i);
  }

  brfs_cache_flush_superblock(&fs->cache_state);

  fs->initialized = 1;

  return BRFS_OK;
}

/* ---- Superblock Validation ---- */

static int brfs_validate_superblock(struct brfs_superblock *sb)
{
  if (sb->magic != BRFS_MAGIC)
  {
    return BRFS_ERR_INVALID_SUPERBLOCK;
  }

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

/* ---- Mount/Unmount Functions ---- */

int brfs_mount(struct brfs_state *fs)
{
  struct brfs_superblock *sb;
  unsigned int data_size;
  unsigned int i;
  int result;

  result = brfs_cache_load_superblock(&fs->cache_state);
  if (result != BRFS_OK) return result;

  sb = (struct brfs_superblock *)brfs_get_superblock(fs);
  result = brfs_validate_superblock(sb);
  if (result != BRFS_OK)
  {
    return result;
  }

  data_size = BRFS_SUPERBLOCK_SIZE + sb->total_blocks +
              (sb->total_blocks * sb->words_per_block);
  if (data_size > fs->cache_size)
  {
    /* FS doesn't fit linearly.  Check if at least LRU mode is possible:
     * we need superblock + FAT + slot_of + at least one data slot. */
    unsigned int min_lru = BRFS_SUPERBLOCK_SIZE +
                           sb->total_blocks * 2u +
                           sb->words_per_block + 4u;
    if (min_lru > fs->cache_size)
    {
      return BRFS_ERR_NO_SPACE;
    }
  }

  brfs_cache_configure(&fs->cache_state, sb->total_blocks, sb->words_per_block);

  result = brfs_cache_load(&fs->cache_state, fs->progress_callback);
  if (result != BRFS_OK) return result;

  for (i = 0; i < BRFS_MAX_OPEN_FILES; i++)
  {
    fs->open_files[i].fat_idx = 0;
    fs->open_files[i].cursor = 0;
    fs->open_files[i].in_use = 0;
  }

  fs->initialized = 1;

  return BRFS_OK;
}

int brfs_unmount(struct brfs_state *fs)
{
  int result;
  unsigned int i;

  if (!fs->initialized)
  {
    return BRFS_ERR_NOT_INITIALIZED;
  }

  result = brfs_sync(fs);
  if (result != BRFS_OK)
  {
    return result;
  }

  for (i = 0; i < BRFS_MAX_OPEN_FILES; i++)
  {
    fs->open_files[i].fat_idx = 0;
    fs->open_files[i].cursor = 0;
    fs->open_files[i].in_use = 0;
  }

  fs->initialized = 0;

  return BRFS_OK;
}

/* ---- Flash Sync Functions ----
 * Per-sector write helpers and brfs_sync's dirty-walking now live in
 * brfs_cache.c. brfs_sync is a thin wrapper. */

int brfs_sync(struct brfs_state *fs)
{
  if (!fs->initialized)
  {
    return BRFS_ERR_NOT_INITIALIZED;
  }
  return brfs_cache_flush(&fs->cache_state, fs->progress_callback);
}

/* ---- File Creation ---- */

int brfs_create_file(struct brfs_state *fs, const char *path)
{
  char dir_path[BRFS_MAX_PATH_LENGTH + 1];
  char filename[BRFS_MAX_FILENAME_LENGTH + 1];
  int result;
  int dir_fat_idx;
  int free_block;
  int free_entry_idx;
  unsigned int *dir_block;
  unsigned int *fat;
  struct brfs_dir_entry *entry;
  struct brfs_dir_entry new_entry;
  struct brfs_superblock *sb;

  if (!fs->initialized)
  {
    return BRFS_ERR_NOT_INITIALIZED;
  }

  result = brfs_parse_path(path, dir_path, filename, sizeof(dir_path));
  if (result != BRFS_OK)
  {
    return result;
  }

  dir_fat_idx = brfs_get_dir_fat_idx(fs, dir_path);
  if (dir_fat_idx < 0)
  {
    return dir_fat_idx;
  }

  result = brfs_find_in_directory(fs, dir_fat_idx, filename, &entry, NULL);
  if (result == BRFS_OK)
  {
    return BRFS_ERR_EXISTS;
  }

  free_block = brfs_find_free_block(fs);
  if (free_block < 0)
  {
    return free_block;
  }

  dir_block = brfs_get_data_block(fs, dir_fat_idx);
  free_entry_idx = brfs_find_free_dir_entry(fs, dir_block);
  if (free_entry_idx < 0)
  {
    return free_entry_idx;
  }

  brfs_create_dir_entry(&new_entry, filename, free_block, 0, 0);

  entry = (struct brfs_dir_entry *)(dir_block + (free_entry_idx * BRFS_DIR_ENTRY_SIZE));
  memcpy(entry, &new_entry, sizeof(struct brfs_dir_entry));

  fat = brfs_get_fat(fs);
  fat[free_block] = BRFS_FAT_EOF;

  brfs_mark_block_dirty(fs, dir_fat_idx);

  sb = (struct brfs_superblock *)brfs_get_superblock(fs);
  memset(brfs_get_data_block(fs, free_block), 0, sb->words_per_block * sizeof(unsigned int));

  brfs_mark_block_dirty(fs, free_block);

  return BRFS_OK;
}

/* ---- Directory Creation ---- */

int brfs_create_dir(struct brfs_state *fs, const char *path)
{
  char dir_path[BRFS_MAX_PATH_LENGTH + 1];
  char dirname[BRFS_MAX_FILENAME_LENGTH + 1];
  int result;
  int parent_fat_idx;
  int free_block;
  int free_entry_idx;
  unsigned int *parent_dir_block;
  unsigned int *new_dir_block;
  unsigned int *fat;
  struct brfs_dir_entry *entry;
  struct brfs_dir_entry new_entry;
  struct brfs_superblock *sb;
  unsigned int max_entries;

  if (!fs->initialized)
  {
    return BRFS_ERR_NOT_INITIALIZED;
  }

  result = brfs_parse_path(path, dir_path, dirname, sizeof(dir_path));
  if (result != BRFS_OK)
  {
    return result;
  }

  parent_fat_idx = brfs_get_dir_fat_idx(fs, dir_path);
  if (parent_fat_idx < 0)
  {
    return parent_fat_idx;
  }

  result = brfs_find_in_directory(fs, parent_fat_idx, dirname, &entry, NULL);
  if (result == BRFS_OK)
  {
    return BRFS_ERR_EXISTS;
  }

  free_block = brfs_find_free_block(fs);
  if (free_block < 0)
  {
    return free_block;
  }

  parent_dir_block = brfs_get_data_block(fs, parent_fat_idx);
  free_entry_idx = brfs_find_free_dir_entry(fs, parent_dir_block);
  if (free_entry_idx < 0)
  {
    return free_entry_idx;
  }

  sb = (struct brfs_superblock *)brfs_get_superblock(fs);
  max_entries = sb->words_per_block / BRFS_DIR_ENTRY_SIZE;

  brfs_create_dir_entry(&new_entry, dirname, free_block,
                        max_entries * BRFS_DIR_ENTRY_SIZE * 4u, BRFS_FLAG_DIRECTORY);

  entry = (struct brfs_dir_entry *)(parent_dir_block + (free_entry_idx * BRFS_DIR_ENTRY_SIZE));
  memcpy(entry, &new_entry, sizeof(struct brfs_dir_entry));

  brfs_mark_block_dirty(fs, parent_fat_idx);

  new_dir_block = brfs_get_data_block(fs, free_block);
  brfs_init_directory_block(fs, new_dir_block, free_block, parent_fat_idx);

  fat = brfs_get_fat(fs);
  fat[free_block] = BRFS_FAT_EOF;

  brfs_mark_block_dirty(fs, free_block);

  return BRFS_OK;
}

/* ---- File Open/Close ---- */

int brfs_open(struct brfs_state *fs, const char *path)
{
  char dir_path[BRFS_MAX_PATH_LENGTH + 1];
  char filename[BRFS_MAX_FILENAME_LENGTH + 1];
  int result;
  int dir_fat_idx;
  struct brfs_dir_entry *entry;
  int fd;
  unsigned int i;
  unsigned int entry_idx;

  if (!fs->initialized)
  {
    return BRFS_ERR_NOT_INITIALIZED;
  }

  result = brfs_parse_path(path, dir_path, filename, sizeof(dir_path));
  if (result != BRFS_OK)
  {
    return result;
  }

  dir_fat_idx = brfs_get_dir_fat_idx(fs, dir_path);
  if (dir_fat_idx < 0)
  {
    return dir_fat_idx;
  }

  result = brfs_find_in_directory(fs, dir_fat_idx, filename, &entry, &entry_idx);
  if (result != BRFS_OK)
  {
    return result;
  }

  if (entry->flags & BRFS_FLAG_DIRECTORY)
  {
    return BRFS_ERR_IS_DIRECTORY;
  }

  for (i = 0; i < BRFS_MAX_OPEN_FILES; i++)
  {
    if (fs->open_files[i].fat_idx == entry->fat_idx &&
        fs->open_files[i].in_use)
    {
      return BRFS_ERR_IS_OPEN;
    }
  }

  fd = -1;
  for (i = 0; i < BRFS_MAX_OPEN_FILES; i++)
  {
    if (!fs->open_files[i].in_use)
    {
      fd = (int)i;
      break;
    }
  }

  if (fd < 0)
  {
    return BRFS_ERR_TOO_MANY_OPEN;
  }

  fs->open_files[fd].fat_idx = entry->fat_idx;
  fs->open_files[fd].cursor = 0;
  fs->open_files[fd].dir_fat_idx = (unsigned int)dir_fat_idx;
  fs->open_files[fd].dir_entry_idx = entry_idx;
  fs->open_files[fd].filesize = entry->filesize;
  fs->open_files[fd].in_use = 1;

  return fd;
}

int brfs_close(struct brfs_state *fs, int fd)
{
  if (!fs->initialized)
  {
    return BRFS_ERR_NOT_INITIALIZED;
  }

  if (fd < 0 || fd >= BRFS_MAX_OPEN_FILES)
  {
    return BRFS_ERR_INVALID_PARAM;
  }

  if (!fs->open_files[fd].in_use)
  {
    return BRFS_ERR_NOT_OPEN;
  }

  /* Write back cached filesize to the on-disk directory entry. */
  {
    unsigned int *dir_block = brfs_get_data_block(fs, fs->open_files[fd].dir_fat_idx);
    struct brfs_dir_entry *de = (struct brfs_dir_entry *)
        (dir_block + (fs->open_files[fd].dir_entry_idx * BRFS_DIR_ENTRY_SIZE));
    if (de->filesize != fs->open_files[fd].filesize)
    {
      de->filesize = fs->open_files[fd].filesize;
      brfs_mark_block_dirty(fs, fs->open_files[fd].dir_fat_idx);
    }
  }

  fs->open_files[fd].fat_idx = 0;
  fs->open_files[fd].cursor = 0;
  fs->open_files[fd].in_use = 0;

  return BRFS_OK;
}

/*
 * brfs_close_all — close all open file descriptors.
 * Called by BDOS after a user program exits (normally or via crash)
 * to ensure no stale file handles remain.
 */
void brfs_close_all(struct brfs_state *fs)
{
  int i;
  if (!fs->initialized)
    return;
  for (i = 0; i < BRFS_MAX_OPEN_FILES; i++)
  {
    if (fs->open_files[i].in_use)
      brfs_close(fs, i);
  }
}

/* ---- File Read (byte-mode) ---- */

int brfs_read(struct brfs_state *fs, int fd, void *buffer, unsigned int length)
{
  struct brfs_superblock *sb;
  struct brfs_file *file;
  unsigned char *out;
  unsigned char *block_bytes;
  unsigned int *fat;
  unsigned int current_fat_idx;
  unsigned int cursor_in_block;
  unsigned int bytes_per_block;
  unsigned int bytes_to_read;
  unsigned int bytes_until_end;
  unsigned int total_read;
  unsigned int remaining;

  if (!fs->initialized) return BRFS_ERR_NOT_INITIALIZED;
  if (fd < 0 || fd >= BRFS_MAX_OPEN_FILES) return BRFS_ERR_INVALID_PARAM;

  file = &fs->open_files[fd];
  if (!file->in_use) return BRFS_ERR_NOT_OPEN;
  if (buffer == NULL) return BRFS_ERR_INVALID_PARAM;

  sb = (struct brfs_superblock *)brfs_get_superblock(fs);
  fat = brfs_get_fat(fs);
  bytes_per_block = sb->words_per_block * 4u;

  if (file->cursor >= file->filesize) return 0;

  remaining = file->filesize - file->cursor;
  if (length > remaining) length = remaining;

  current_fat_idx = brfs_get_fat_idx_at_offset(fs, file->fat_idx, file->cursor);
  if ((int)current_fat_idx < 0) return BRFS_ERR_READ_ERROR;

  out = (unsigned char *)buffer;
  total_read = 0;

  while (length > 0)
  {
    cursor_in_block = file->cursor % bytes_per_block;
    bytes_until_end = bytes_per_block - cursor_in_block;
    bytes_to_read = (bytes_until_end < length) ? bytes_until_end : length;

    block_bytes = (unsigned char *)brfs_get_data_block(fs, current_fat_idx);
    memcpy(out, block_bytes + cursor_in_block, bytes_to_read);

    out += bytes_to_read;
    file->cursor += bytes_to_read;
    total_read += bytes_to_read;
    length -= bytes_to_read;

    if (length > 0)
    {
      current_fat_idx = fat[current_fat_idx];
      if (current_fat_idx == BRFS_FAT_EOF) break;
    }
  }

  return (int)total_read;
}

/* ---- File Write (byte-mode) ---- */

int brfs_write(struct brfs_state *fs, int fd, const void *buffer, unsigned int length)
{
  struct brfs_superblock *sb;
  struct brfs_file *file;
  const unsigned char *in;
  unsigned char *block_bytes;
  unsigned int *fat;
  unsigned int current_fat_idx;
  unsigned int cursor_in_block;
  unsigned int bytes_per_block;
  unsigned int bytes_to_write;
  unsigned int bytes_until_end;
  unsigned int total_written;
  int next_block;
  int result;

  if (!fs->initialized) return BRFS_ERR_NOT_INITIALIZED;
  if (fd < 0 || fd >= BRFS_MAX_OPEN_FILES) return BRFS_ERR_INVALID_PARAM;

  file = &fs->open_files[fd];
  if (!file->in_use) return BRFS_ERR_NOT_OPEN;
  if (buffer == NULL && length > 0) return BRFS_ERR_INVALID_PARAM;
  if (length == 0) return 0;

  sb = (struct brfs_superblock *)brfs_get_superblock(fs);
  fat = brfs_get_fat(fs);
  bytes_per_block = sb->words_per_block * 4u;

  result = brfs_get_fat_idx_at_offset(fs, file->fat_idx, file->cursor);
  if (result < 0)
  {
    /* Cursor sits exactly at the end of the last block (filesize is a
     * multiple of bytes_per_block). Allocate a fresh block. */
    if (file->cursor == file->filesize &&
        (file->cursor % bytes_per_block) == 0 &&
        file->cursor > 0)
    {
      unsigned int last_idx;
      last_idx = file->fat_idx;
      while (fat[last_idx] != BRFS_FAT_EOF)
      {
        last_idx = fat[last_idx];
      }
      next_block = brfs_find_free_block(fs);
      if (next_block < 0) return BRFS_ERR_NO_SPACE;
      fat[last_idx] = next_block;
      fat[next_block] = BRFS_FAT_EOF;
      memset(brfs_get_data_block(fs, next_block), 0, sb->words_per_block * sizeof(unsigned int));
      brfs_mark_block_dirty(fs, next_block);
      current_fat_idx = (unsigned int)next_block;
    }
    else
    {
      return BRFS_ERR_SEEK_ERROR;
    }
  }
  else
  {
    current_fat_idx = (unsigned int)result;
  }

  in = (const unsigned char *)buffer;
  total_written = 0;

  while (length > 0)
  {
    cursor_in_block = file->cursor % bytes_per_block;
    bytes_until_end = bytes_per_block - cursor_in_block;
    bytes_to_write = (bytes_until_end < length) ? bytes_until_end : length;

    block_bytes = (unsigned char *)brfs_get_data_block(fs, current_fat_idx);
    memcpy(block_bytes + cursor_in_block, in, bytes_to_write);

    brfs_mark_block_dirty(fs, current_fat_idx);

    in += bytes_to_write;
    file->cursor += bytes_to_write;
    total_written += bytes_to_write;
    length -= bytes_to_write;

    if (length > 0)
    {
      if (fat[current_fat_idx] == BRFS_FAT_EOF)
      {
        next_block = brfs_find_free_block(fs);
        if (next_block < 0)
        {
          if (file->cursor > file->filesize)
          {
            file->filesize = file->cursor;
          }
          return (int)total_written;
        }
        fat[current_fat_idx] = next_block;
        fat[next_block] = BRFS_FAT_EOF;
        memset(brfs_get_data_block(fs, next_block), 0, sb->words_per_block * sizeof(unsigned int));
        brfs_mark_block_dirty(fs, next_block);
        current_fat_idx = next_block;
      }
      else
      {
        current_fat_idx = fat[current_fat_idx];
      }
    }
  }

  if (file->cursor > file->filesize)
  {
    file->filesize = file->cursor;
  }

  return (int)total_written;
}

/* ---- File Seek/Tell ---- */

int brfs_seek(struct brfs_state *fs, int fd, unsigned int offset)
{
  struct brfs_file *file;

  if (!fs->initialized)
  {
    return BRFS_ERR_NOT_INITIALIZED;
  }

  if (fd < 0 || fd >= BRFS_MAX_OPEN_FILES)
  {
    return BRFS_ERR_INVALID_PARAM;
  }

  file = &fs->open_files[fd];

  if (!file->in_use)
  {
    return BRFS_ERR_NOT_OPEN;
  }

  if (offset > file->filesize)
  {
    offset = file->filesize;
  }

  file->cursor = offset;

  return (int)offset;
}

int brfs_tell(struct brfs_state *fs, int fd)
{
  struct brfs_file *file;

  if (!fs->initialized)
  {
    return BRFS_ERR_NOT_INITIALIZED;
  }

  if (fd < 0 || fd >= BRFS_MAX_OPEN_FILES)
  {
    return BRFS_ERR_INVALID_PARAM;
  }

  file = &fs->open_files[fd];

  if (!file->in_use)
  {
    return BRFS_ERR_NOT_OPEN;
  }

  return (int)file->cursor;
}

int brfs_file_size(struct brfs_state *fs, int fd)
{
  struct brfs_file *file;

  if (!fs->initialized)
  {
    return BRFS_ERR_NOT_INITIALIZED;
  }

  if (fd < 0 || fd >= BRFS_MAX_OPEN_FILES)
  {
    return BRFS_ERR_INVALID_PARAM;
  }

  file = &fs->open_files[fd];

  if (!file->in_use)
  {
    return BRFS_ERR_NOT_OPEN;
  }

  return (int)file->filesize;
}

/* ---- Directory Reading ---- */

int brfs_read_dir(struct brfs_state *fs, const char *path, struct brfs_dir_entry *buffer, unsigned int max_entries)
{
  struct brfs_superblock *sb;
  int dir_fat_idx;
  unsigned int *dir_block;
  unsigned int dir_max_entries;
  unsigned int count;
  unsigned int i;
  struct brfs_dir_entry *entry;

  if (!fs->initialized)
  {
    return BRFS_ERR_NOT_INITIALIZED;
  }

  if (buffer == NULL)
  {
    return BRFS_ERR_INVALID_PARAM;
  }

  dir_fat_idx = brfs_get_dir_fat_idx(fs, path);
  if (dir_fat_idx < 0)
  {
    return dir_fat_idx;
  }

  sb = (struct brfs_superblock *)brfs_get_superblock(fs);
  dir_block = brfs_get_data_block(fs, dir_fat_idx);
  dir_max_entries = sb->words_per_block / BRFS_DIR_ENTRY_SIZE;

  count = 0;

  for (i = 0; i < dir_max_entries && count < max_entries; i++)
  {
    entry = (struct brfs_dir_entry *)(dir_block + (i * BRFS_DIR_ENTRY_SIZE));

    if (entry->filename[0] != 0)
    {
      memcpy(&buffer[count], entry, sizeof(struct brfs_dir_entry));
      count++;
    }
  }

  return (int)count;
}

/* ---- Delete ---- */

int brfs_delete(struct brfs_state *fs, const char *path)
{
  char dir_path[BRFS_MAX_PATH_LENGTH + 1];
  char filename[BRFS_MAX_FILENAME_LENGTH + 1];
  int result;
  int dir_fat_idx;
  struct brfs_dir_entry *entry;
  unsigned int entry_idx;
  unsigned int entry_fat_idx;
  unsigned int entry_flags;
  unsigned int *fat;
  unsigned int current_fat_idx;
  unsigned int next_fat_idx;
  unsigned int i;

  if (!fs->initialized)
  {
    return BRFS_ERR_NOT_INITIALIZED;
  }

  result = brfs_parse_path(path, dir_path, filename, sizeof(dir_path));
  if (result != BRFS_OK)
  {
    return result;
  }

  dir_fat_idx = brfs_get_dir_fat_idx(fs, dir_path);
  if (dir_fat_idx < 0)
  {
    return dir_fat_idx;
  }

  result = brfs_find_in_directory(fs, dir_fat_idx, filename, &entry, &entry_idx);
  if (result != BRFS_OK)
  {
    return result;
  }

  /* Snapshot entry fields now — the entry pointer may become stale
   * after subsequent brfs_get_data_block calls in LRU mode. */
  entry_fat_idx = entry->fat_idx;
  entry_flags   = entry->flags;

  if (entry_flags & BRFS_FLAG_DIRECTORY)
  {
    struct brfs_superblock *sb;
    unsigned int *target_dir_block;
    unsigned int max_entries;
    struct brfs_dir_entry *sub_entry;
    unsigned int non_empty_count;

    sb = (struct brfs_superblock *)brfs_get_superblock(fs);
    target_dir_block = brfs_get_data_block(fs, entry_fat_idx);
    max_entries = sb->words_per_block / BRFS_DIR_ENTRY_SIZE;

    non_empty_count = 0;
    for (i = 0; i < max_entries; i++)
    {
      sub_entry = (struct brfs_dir_entry *)(target_dir_block + (i * BRFS_DIR_ENTRY_SIZE));
      if (sub_entry->filename[0] != 0)
      {
        non_empty_count++;
      }
    }

    if (non_empty_count > 2)
    {
      return BRFS_ERR_NOT_EMPTY;
    }
  }

  for (i = 0; i < BRFS_MAX_OPEN_FILES; i++)
  {
    if (fs->open_files[i].in_use && fs->open_files[i].fat_idx == entry_fat_idx)
    {
      return BRFS_ERR_IS_OPEN;
    }
  }

  fat = brfs_get_fat(fs);
  current_fat_idx = entry_fat_idx;

  while (current_fat_idx != BRFS_FAT_EOF)
  {
    next_fat_idx = fat[current_fat_idx];
    fat[current_fat_idx] = BRFS_FAT_FREE;
    brfs_mark_block_dirty(fs, current_fat_idx);
    current_fat_idx = next_fat_idx;
  }

  /* Re-fetch the directory block — the original entry pointer may have
   * been invalidated by brfs_get_data_block calls above (LRU eviction). */
  {
    unsigned int *dir_block = brfs_get_data_block(fs, dir_fat_idx);
    entry = (struct brfs_dir_entry *)(dir_block + (entry_idx * BRFS_DIR_ENTRY_SIZE));
  }
  memset(entry, 0, sizeof(struct brfs_dir_entry));

  brfs_mark_block_dirty(fs, dir_fat_idx);

  return BRFS_OK;
}

/* ---- Stat Functions ---- */

int brfs_stat(struct brfs_state *fs, const char *path, struct brfs_dir_entry *entry)
{
  char dir_path[BRFS_MAX_PATH_LENGTH + 1];
  char filename[BRFS_MAX_FILENAME_LENGTH + 1];
  int result;
  int dir_fat_idx;
  struct brfs_dir_entry *found_entry;
  int len;

  if (!fs->initialized)
  {
    return BRFS_ERR_NOT_INITIALIZED;
  }

  if (entry == NULL)
  {
    return BRFS_ERR_INVALID_PARAM;
  }

  len = strlen(path);
  if (len == 0 || (len == 1 && path[0] == '/'))
  {
    struct brfs_superblock *sb;
    sb = (struct brfs_superblock *)brfs_get_superblock(fs);

    memset(entry, 0, sizeof(struct brfs_dir_entry));
    entry->flags = BRFS_FLAG_DIRECTORY;
    entry->fat_idx = 0;
    entry->filesize = sb->words_per_block * 4u;
    brfs_compress_string(entry->filename, "/");
    return BRFS_OK;
  }

  result = brfs_parse_path(path, dir_path, filename, sizeof(dir_path));
  if (result != BRFS_OK)
  {
    return result;
  }

  dir_fat_idx = brfs_get_dir_fat_idx(fs, dir_path);
  if (dir_fat_idx < 0)
  {
    return dir_fat_idx;
  }

  result = brfs_find_in_directory(fs, dir_fat_idx, filename, &found_entry, NULL);
  if (result != BRFS_OK)
  {
    return result;
  }

  memcpy(entry, found_entry, sizeof(struct brfs_dir_entry));

  return BRFS_OK;
}

int brfs_exists(struct brfs_state *fs, const char *path)
{
  struct brfs_dir_entry entry;
  return (brfs_stat(fs, path, &entry) == BRFS_OK) ? 1 : 0;
}

int brfs_is_dir(struct brfs_state *fs, const char *path)
{
  struct brfs_dir_entry entry;

  if (brfs_stat(fs, path, &entry) != BRFS_OK)
  {
    return 0;
  }

  return (entry.flags & BRFS_FLAG_DIRECTORY) ? 1 : 0;
}

/* ---- Filesystem Statistics ---- */

int brfs_statfs(struct brfs_state *fs, unsigned int *total_blocks, unsigned int *free_blocks,
                unsigned int *block_size)
{
  struct brfs_superblock *sb;
  unsigned int *fat;
  unsigned int free_count;
  unsigned int i;

  if (!fs->initialized)
  {
    return BRFS_ERR_NOT_INITIALIZED;
  }

  sb = (struct brfs_superblock *)brfs_get_superblock(fs);
  fat = brfs_get_fat(fs);

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

int brfs_get_label(struct brfs_state *fs, char *label_buffer, unsigned int buffer_size)
{
  struct brfs_superblock *sb;
  unsigned int i;

  if (!fs->initialized)
  {
    return BRFS_ERR_NOT_INITIALIZED;
  }

  if (label_buffer == NULL || buffer_size == 0)
  {
    return BRFS_ERR_INVALID_PARAM;
  }

  sb = (struct brfs_superblock *)brfs_get_superblock(fs);

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

/* ---- Error Strings ---- */

const char *brfs_strerror(int error_code)
{
  switch (error_code)
  {
  case BRFS_OK:
    return "Success";
  case BRFS_ERR_INVALID_PARAM:
    return "Invalid parameter";
  case BRFS_ERR_NOT_FOUND:
    return "Not found";
  case BRFS_ERR_EXISTS:
    return "Already exists";
  case BRFS_ERR_NO_SPACE:
    return "No space left";
  case BRFS_ERR_NO_ENTRY:
    return "No free directory entry";
  case BRFS_ERR_NOT_EMPTY:
    return "Directory not empty";
  case BRFS_ERR_IS_OPEN:
    return "File is open";
  case BRFS_ERR_NOT_OPEN:
    return "File is not open";
  case BRFS_ERR_TOO_MANY_OPEN:
    return "Too many open files";
  case BRFS_ERR_IS_DIRECTORY:
    return "Is a directory";
  case BRFS_ERR_NOT_DIRECTORY:
    return "Not a directory";
  case BRFS_ERR_PATH_TOO_LONG:
    return "Path too long";
  case BRFS_ERR_NAME_TOO_LONG:
    return "Filename too long";
  case BRFS_ERR_INVALID_SUPERBLOCK:
    return "Invalid superblock";
  case BRFS_ERR_FLASH_ERROR:
    return "Flash error";
  case BRFS_ERR_SEEK_ERROR:
    return "Seek error";
  case BRFS_ERR_READ_ERROR:
    return "Read error";
  case BRFS_ERR_WRITE_ERROR:
    return "Write error";
  case BRFS_ERR_NOT_INITIALIZED:
    return "Not initialized";
  default:
    return "Unknown error";
  }
}
