#include "bdos.h"
#include "brfs_storage_spi_flash.h"
#include "brfs_storage_sdcard.h"
#include "sd.h"

static brfs_spi_flash_storage_t bdos_fs_storage;
static brfs_sdcard_storage_t    bdos_sd_storage;

/* Global BRFS instance for SPI flash filesystem */
struct brfs_state brfs_spi;

/* Global BRFS instance for SD card filesystem */
struct brfs_state brfs_sd;

int bdos_fs_ready = 0;
int bdos_fs_boot_needs_format = 0;
int bdos_fs_last_mount_error = BRFS_OK;
int bdos_sd_ready = 0;       /* FS mounted and usable */
int bdos_sd_initialized = 0; /* hardware init + brfs_init OK (can format) */

static char bdos_fs_progress_label[12] = "";
static int bdos_fs_progress_last_pct = -1;

static void bdos_fs_progress_reset(void)
{
  bdos_fs_progress_label[0] = '\0';
  bdos_fs_progress_last_pct = -1;
}

static void bdos_fs_render_progress(char *label, unsigned int current, unsigned int total)
{
  int percent;
  int i;
  int fill;

  if (total == 0)
  {
    total = 1;
  }

  percent = (int)((current * 100) / total);
  if (percent > 100)
  {
    percent = 100;
  }

  if (strcmp(label, bdos_fs_progress_label) != 0)
  {
    if (bdos_fs_progress_label[0] != '\0' && bdos_fs_progress_last_pct != 100)
    {
      term_putchar('\n');
    }
    strcpy(bdos_fs_progress_label, label);
    bdos_fs_progress_last_pct = -1;
  }

  if (percent == bdos_fs_progress_last_pct)
  {
    return;
  }

  bdos_fs_progress_last_pct = percent;

  term_putchar('\r');
  term_puts(label);
  term_puts(" [");

  fill = (percent * 20) / 100;
  for (i = 0; i < 20; i++)
  {
    if (i < fill)
    {
      term_putchar('#');
    }
    else
    {
      term_putchar('.');
    }
  }

  term_puts("] ");
  if (percent < 10)
  {
    term_puts("  ");
  }
  else if (percent < 100)
  {
    term_putchar(' ');
  }
  term_putint(percent);
  term_putchar('%');

  if (percent == 100)
  {
    term_putchar('\n');
  }
}

static void bdos_fs_progress_callback(const char *phase, unsigned int current, unsigned int total)
{
  if (strncmp(phase, "mount", 5) == 0)
  {
    bdos_fs_render_progress("mount ", current, total);
    return;
  }

  if (strncmp(phase, "format", 6) == 0)
  {
    bdos_fs_render_progress("format", current, total);
    return;
  }

  if (strncmp(phase, "sync", 4) == 0)
  {
    bdos_fs_render_progress("sync  ", current, total);
    return;
  }

  bdos_fs_render_progress("fs    ", current, total);
}

void bdos_fs_boot_init(void)
{
  int result;

  brfs_storage_spi_flash_init(&bdos_fs_storage, BDOS_FS_FLASH_ID);
  result = brfs_init(&brfs_spi, &bdos_fs_storage.base, (unsigned int *)MEM_BRFS_START, MEM_BRFS_SIZE / sizeof(unsigned int));
  if (result != BRFS_OK)
  {
    bdos_panic("Failed to initialize BRFS subsystem");
    return;
  }

  bdos_fs_progress_reset();
  brfs_set_progress_callback(&brfs_spi, bdos_fs_progress_callback);
  result = brfs_mount(&brfs_spi);
  brfs_set_progress_callback(&brfs_spi, NULL);

  if (result == BRFS_OK)
  {
    bdos_fs_ready = 1;
    bdos_fs_boot_needs_format = 0;
    bdos_fs_last_mount_error = BRFS_OK;
    return;
  }

  bdos_fs_ready = 0;
  bdos_fs_boot_needs_format = 1;
  bdos_fs_last_mount_error = result;
}

void bdos_fs_sd_init(void)
{
  sd_card_info_t info;
  int result;

  if (sd_init(&info) != SD_OK)
  {
    bdos_sd_ready = 0;
    bdos_sd_initialized = 0;
    return;
  }

  brfs_storage_sdcard_init(&bdos_sd_storage);
  result = brfs_init(&brfs_sd, &bdos_sd_storage.base,
                     (unsigned int *)MEM_SD_CACHE_START,
                     MEM_SD_CACHE_SIZE / sizeof(unsigned int));
  if (result != BRFS_OK)
  {
    bdos_sd_ready = 0;
    bdos_sd_initialized = 0;
    return;
  }

  /* Hardware + BRFS subsystem ready — can format even if mount fails */
  bdos_sd_initialized = 1;

  bdos_fs_progress_reset();
  brfs_set_progress_callback(&brfs_sd, bdos_fs_progress_callback);
  result = brfs_mount(&brfs_sd);
  brfs_set_progress_callback(&brfs_sd, NULL);

  if (result == BRFS_OK)
  {
    bdos_sd_ready = 1;
    if (brfs_sd.cache_state.lru_enabled)
    {
      term_puts("SD card mounted at /sdcard (LRU cache, ");
      term_putint((int)brfs_sd.cache_state.num_slots);
      term_puts(" slots)\n");
    }
    else
    {
      term_puts("SD card mounted at /sdcard\n");
    }
  }
  else
  {
    bdos_sd_ready = 0;
    term_puts("SD card could not be mounted\n");
  }
}

int bdos_fs_format_and_sync(unsigned int total_blocks, unsigned int words_per_block,
                            char *label, int full_format)
{
  int result;

  bdos_fs_progress_reset();
  brfs_set_progress_callback(&brfs_spi, bdos_fs_progress_callback);
  result = brfs_format(&brfs_spi, total_blocks, words_per_block, label, full_format);
  if (result != BRFS_OK)
  {
    brfs_set_progress_callback(&brfs_spi, NULL);
    bdos_fs_ready = 0;
    return result;
  }

  result = brfs_sync(&brfs_spi);
  brfs_set_progress_callback(&brfs_spi, NULL);
  if (result != BRFS_OK)
  {
    bdos_fs_ready = 0;
    return result;
  }

  bdos_fs_ready = 1;
  bdos_fs_boot_needs_format = 0;
  bdos_fs_last_mount_error = BRFS_OK;

  return BRFS_OK;
}

int bdos_fs_sd_format_and_sync(unsigned int total_blocks, unsigned int words_per_block,
                               char *label, int full_format)
{
  int result;
  unsigned int needed_words;

  if (!bdos_sd_initialized)
  {
    return BRFS_ERR_NOT_INITIALIZED;
  }

  /* Verify the requested FS fits in the SD cache buffer (linearly or via LRU). */
  needed_words = BRFS_SUPERBLOCK_SIZE + total_blocks +
                 total_blocks * words_per_block;
  if (needed_words > brfs_sd.cache_size)
  {
    /* Doesn't fit linearly — check LRU minimum (sb + FAT + slot_of + 1 slot). */
    unsigned int min_lru = BRFS_SUPERBLOCK_SIZE +
                           total_blocks * 2u +
                           words_per_block + 4u;
    if (min_lru > brfs_sd.cache_size)
    {
      return BRFS_ERR_INVALID_PARAM;
    }
  }

  bdos_fs_progress_reset();
  brfs_set_progress_callback(&brfs_sd, bdos_fs_progress_callback);
  result = brfs_format(&brfs_sd, total_blocks, words_per_block, label, full_format);
  if (result != BRFS_OK)
  {
    brfs_set_progress_callback(&brfs_sd, NULL);
    return result;
  }

  result = brfs_sync(&brfs_sd);
  brfs_set_progress_callback(&brfs_sd, NULL);
  if (result != BRFS_OK)
  {
    return result;
  }

  /* Format succeeded — the SD FS is now mounted and usable */
  bdos_sd_ready = 1;

  return BRFS_OK;
}

int bdos_fs_sync_now(void)
{
  int result;

  if (!bdos_fs_ready)
  {
    return BRFS_ERR_NOT_INITIALIZED;
  }

  bdos_fs_progress_reset();
  brfs_set_progress_callback(&brfs_spi, bdos_fs_progress_callback);
  result = brfs_sync(&brfs_spi);
  brfs_set_progress_callback(&brfs_spi, NULL);

  if (result != BRFS_OK)
  {
    return result;
  }

  /* Sync SD card if mounted */
  if (bdos_sd_ready)
  {
    bdos_fs_progress_reset();
    brfs_set_progress_callback(&brfs_sd, bdos_fs_progress_callback);
    result = brfs_sync(&brfs_sd);
    brfs_set_progress_callback(&brfs_sd, NULL);
  }

  return result;
}

char *bdos_fs_error_string(int error_code)
{
  return (char *)brfs_strerror(error_code);
}

struct brfs_state *bdos_fs_for_path(const char *path, const char **rel_path)
{
  /* Check for /sdcard prefix */
  if (path[0] == '/' && path[1] == 's' && path[2] == 'd' &&
      path[3] == 'c' && path[4] == 'a' && path[5] == 'r' &&
      path[6] == 'd')
  {
    if (path[7] == '/' || path[7] == '\0')
    {
      if (bdos_sd_ready)
      {
        *rel_path = (path[7] == '\0') ? "/" : path + 7;
        return &brfs_sd;
      }
    }
  }
  *rel_path = path;
  return &brfs_spi;
}
