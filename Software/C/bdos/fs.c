#include "bdos.h"
#include "brfs_storage_spi_flash.h"
#include "enc28j60.h"

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

  term_puts("Initializing BRFS\n");

  brfs_storage_spi_flash_init(&bdos_fs_storage, BDOS_FS_FLASH_ID);
  result = brfs_init(&bdos_fs_storage.base, (unsigned int *)MEM_BRFS_START, MEM_BRFS_SIZE / sizeof(unsigned int));
  if (result != BRFS_OK)
  {
    bdos_panic("Failed to initialize BRFS subsystem");
    return;
  }

  bdos_fs_progress_reset();
  brfs_set_progress_callback(bdos_fs_progress_callback);
  uart_puts("[bdos] brfs_mount() begin\n");
  /* DEBUG: mask ENC28J60 interrupts at the chip level for the duration
   * of the mount, to test whether ENC ISR activity is what hangs SPI1
   * DMA on 4 KiB transfers. */
  enc28j60_isr_begin();
  /* DEBUG: also cancel any active periodic timers. The hang is BDOS-only;
   * baremetal Step 10 (same byte sequence) passes. Timer ISRs are the
   * remaining live interrupt source during the spin. */
  timer_cancel(TIMER_0);
  timer_cancel(TIMER_1);
  timer_cancel(TIMER_2);
  result = brfs_mount();
  enc28j60_isr_end();
  uart_puts("[bdos] brfs_mount() returned ");
  uart_putint(result);
  uart_puts("\n");
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
