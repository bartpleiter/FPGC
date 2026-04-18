#include "bdos.h"
#include "brfs_storage_spi_flash.h"

static brfs_spi_flash_storage_t bdos_fs_storage;

int bdos_fs_ready = 0;
int bdos_fs_boot_needs_format = 0;
int bdos_fs_last_mount_error = BRFS_OK;

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
      term2_putchar('\n');
    }
    strcpy(bdos_fs_progress_label, label);
    bdos_fs_progress_last_pct = -1;
  }

  if (percent == bdos_fs_progress_last_pct)
  {
    return;
  }

  bdos_fs_progress_last_pct = percent;

  term2_putchar('\r');
  term2_puts(label);
  term2_puts(" [");

  fill = (percent * 20) / 100;
  for (i = 0; i < 20; i++)
  {
    if (i < fill)
    {
      term2_putchar('#');
    }
    else
    {
      term2_putchar('.');
    }
  }

  term2_puts("] ");
  if (percent < 10)
  {
    term2_puts("  ");
  }
  else if (percent < 100)
  {
    term2_putchar(' ');
  }
  term2_putint(percent);
  term2_putchar('%');

  if (percent == 100)
  {
    term2_putchar('\n');
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

  term2_puts("Initializing BRFS\n");

  brfs_storage_spi_flash_init(&bdos_fs_storage, BDOS_FS_FLASH_ID);
  result = brfs_init(&bdos_fs_storage.base, (unsigned int *)MEM_BRFS_START, MEM_BRFS_SIZE / sizeof(unsigned int));
  if (result != BRFS_OK)
  {
    bdos_panic("Failed to initialize BRFS subsystem");
    return;
  }

  bdos_fs_progress_reset();
  brfs_set_progress_callback(bdos_fs_progress_callback);
  result = brfs_mount();
  brfs_set_progress_callback(NULL);

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

int bdos_fs_format_and_sync(unsigned int total_blocks, unsigned int words_per_block,
                            char *label, int full_format)
{
  int result;

  bdos_fs_progress_reset();
  brfs_set_progress_callback(bdos_fs_progress_callback);
  result = brfs_format(total_blocks, words_per_block, label, full_format);
  if (result != BRFS_OK)
  {
    brfs_set_progress_callback(NULL);
    bdos_fs_ready = 0;
    return result;
  }

  result = brfs_sync();
  brfs_set_progress_callback(NULL);
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

int bdos_fs_sync_now(void)
{
  int result;

  if (!bdos_fs_ready)
  {
    return BRFS_ERR_NOT_INITIALIZED;
  }

  bdos_fs_progress_reset();
  brfs_set_progress_callback(bdos_fs_progress_callback);
  result = brfs_sync();
  brfs_set_progress_callback(NULL);

  return result;
}

char *bdos_fs_error_string(int error_code)
{
  return (char *)brfs_strerror(error_code);
}
