#include "bdos.h"

#define BDOS_SHELL_FORMAT_LABEL_MAX 10

#define BDOS_SHELL_MODE_NORMAL            0
#define BDOS_SHELL_MODE_BOOT_FORMAT_YN    1
#define BDOS_SHELL_MODE_FORMAT_BLOCKS     2
#define BDOS_SHELL_MODE_FORMAT_WORDS      3
#define BDOS_SHELL_MODE_FORMAT_LABEL      4
#define BDOS_SHELL_MODE_FORMAT_FULL       5

static int bdos_shell_mode = BDOS_SHELL_MODE_NORMAL;
static unsigned int bdos_shell_format_blocks = 0;
static unsigned int bdos_shell_format_words = 0;
static char bdos_shell_format_label[BDOS_SHELL_FORMAT_LABEL_MAX + 1];
static int bdos_shell_format_full = 0;

void bdos_shell_start_format_wizard(void)
{
  bdos_shell_mode = BDOS_SHELL_MODE_FORMAT_BLOCKS;
  term_puts("BRFS v2 format wizard\n");
  term_puts("Enter total blocks (multiple of 64):\n");
}

static void bdos_shell_finish_format_wizard(void)
{
  int result;

  result = bdos_fs_format_and_sync(
    bdos_shell_format_blocks,
    bdos_shell_format_words,
    bdos_shell_format_label,
    bdos_shell_format_full);

  if (result != BRFS_OK)
  {
    bdos_shell_print_fs_error("format/sync", result);
    bdos_shell_mode = BDOS_SHELL_MODE_NORMAL;
    return;
  }

  bdos_shell_mode = BDOS_SHELL_MODE_NORMAL;
}

int bdos_shell_handle_special_mode_line(char *line)
{
  int value;
  int yn;

  if (bdos_shell_mode == BDOS_SHELL_MODE_NORMAL)
  {
    return 0;
  }

  bdos_shell_trim_whitespace(line);

  if (bdos_shell_mode == BDOS_SHELL_MODE_BOOT_FORMAT_YN)
  {
    if (bdos_shell_parse_yes_no(line, &yn) != 0)
    {
      term_puts("Please answer yes or no.\n");
      return 1;
    }

    if (yn)
    {
      bdos_shell_start_format_wizard();
      return 1;
    }

    bdos_panic("Filesystem mount failed and format was declined. BDOS requires a filesystem.");
    return 1;
  }

  if (bdos_shell_mode == BDOS_SHELL_MODE_FORMAT_BLOCKS)
  {
    value = atoi(line);
    if (value <= 0)
    {
      term_puts("Invalid block count. Please enter a positive integer.\n");
      return 1;
    }

    bdos_shell_format_blocks = (unsigned int)value;
    bdos_shell_mode = BDOS_SHELL_MODE_FORMAT_WORDS;
    term_puts("Enter bytes per block (multiple of 256):\n");
    return 1;
  }

  if (bdos_shell_mode == BDOS_SHELL_MODE_FORMAT_WORDS)
  {
    value = atoi(line);
    if (value <= 0)
    {
      term_puts("Invalid bytes-per-block. Please enter a positive integer.\n");
      return 1;
    }

    if ((value & 3) != 0)
    {
      term_puts("Bytes per block must be a multiple of 4.\n");
      return 1;
    }

    bdos_shell_format_words = (unsigned int)value / 4u;
    bdos_shell_mode = BDOS_SHELL_MODE_FORMAT_LABEL;
    term_puts("Enter label (max 10 chars):\n");
    return 1;
  }

  if (bdos_shell_mode == BDOS_SHELL_MODE_FORMAT_LABEL)
  {
    if (strlen(line) == 0)
    {
      term_puts("Label cannot be empty.\n");
      return 1;
    }

    strncpy(bdos_shell_format_label, line, BDOS_SHELL_FORMAT_LABEL_MAX);
    bdos_shell_format_label[BDOS_SHELL_FORMAT_LABEL_MAX] = '\0';

    bdos_shell_mode = BDOS_SHELL_MODE_FORMAT_FULL;
    term_puts("Full format? (yes/no):\n");
    return 1;
  }

  if (bdos_shell_mode == BDOS_SHELL_MODE_FORMAT_FULL)
  {
    if (bdos_shell_parse_yes_no(line, &yn) != 0)
    {
      term_puts("Please answer yes or no.\n");
      return 1;
    }

    bdos_shell_format_full = yn;
    bdos_shell_finish_format_wizard();
    return 1;
  }

  return 0;
}

void bdos_shell_on_startup(void)
{
  if (bdos_fs_ready)
  {
    return;
  }

  term_puts("BRFS mount failed: ");
  term_puts(bdos_fs_error_string(bdos_fs_last_mount_error));
  term_putchar('\n');

  if (bdos_fs_boot_needs_format)
  {
    term_puts("Filesystem is required. Format now? (yes/no)\n");
    bdos_shell_mode = BDOS_SHELL_MODE_BOOT_FORMAT_YN;
  }
}
