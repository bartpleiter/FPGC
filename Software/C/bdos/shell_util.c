#include "bdos.h"

int bdos_shell_require_fs_ready(void)
{
  if (!bdos_fs_ready)
  {
    term2_puts("error: filesystem not mounted\n");
    return 0;
  }

  return 1;
}

void bdos_shell_print_fs_error(char *action, int result)
{
  term2_puts("error: ");
  term2_puts(action);
  term2_puts(" failed: ");
  term2_puts(bdos_fs_error_string(result));
  term2_putchar('\n');
}

void bdos_shell_print_2digit(unsigned int value)
{
  if (value < 10)
  {
    term2_putchar('0');
  }
  term2_putint((int)value);
}

int bdos_shell_u32_to_str(unsigned int value, char *out)
{
  char temp[11];
  int len;
  int i;

  if (value == 0)
  {
    out[0] = '0';
    out[1] = '\0';
    return 1;
  }

  len = 0;
  while (value > 0)
  {
    temp[len] = (char)('0' + (value % 10));
    value = value / 10;
    len++;
  }

  for (i = 0; i < len; i++)
  {
    out[i] = temp[len - 1 - i];
  }
  out[len] = '\0';
  return len;
}

unsigned int bdos_shell_bytes_to_kib_1dp(unsigned int bytes)
{
  return (bytes * 10) / 1024;
}

void bdos_shell_print_kib(unsigned int bytes)
{
  unsigned int kib_1dp;

  kib_1dp = bdos_shell_bytes_to_kib_1dp(bytes);
  term2_putint((int)(kib_1dp / 10));
  term2_putchar('.');
  term2_putint((int)(kib_1dp % 10));
  term2_puts(" KiB");
}

void bdos_shell_print_hline(unsigned int length)
{
  unsigned int i;

  for (i = 0; i < length; i++)
  {
    term2_putchar('-');
  }
  term2_putchar('\n');
}

void bdos_shell_print_field_prefix(char *name, int value_col)
{
  int len;
  int i;

  term2_puts(name);
  len = strlen(name);
  for (i = len; i < value_col; i++)
  {
    term2_putchar(' ');
  }
}

int bdos_shell_format_byte_size(unsigned int bytes, char *out)
{
  int len;
  unsigned int kib_1dp;

  if (bytes >= 1024)
  {
    kib_1dp = (bytes * 10) / 1024;
    len = 0;
    len += bdos_shell_u32_to_str(kib_1dp / 10, out + len);
    out[len++] = '.';
    out[len++] = (char)('0' + (kib_1dp % 10));
    out[len++] = ' ';
    out[len++] = 'K';
    out[len++] = 'i';
    out[len++] = 'B';
    out[len] = '\0';
    return len;
  }

  len = bdos_shell_u32_to_str(bytes, out);
  out[len++] = ' ';
  out[len++] = 'B';
  out[len] = '\0';
  return len;
}

void bdos_shell_sort_names(char names[][BRFS_MAX_FILENAME_LENGTH + 1], int count)
{
  int i;
  int j;
  char tmp[BRFS_MAX_FILENAME_LENGTH + 1];

  for (i = 0; i < count; i++)
  {
    for (j = i + 1; j < count; j++)
    {
      if (strcmp(names[i], names[j]) > 0)
      {
        strcpy(tmp, names[i]);
        strcpy(names[i], names[j]);
        strcpy(names[j], tmp);
      }
    }
  }
}

void bdos_shell_sort_files(char names[][BRFS_MAX_FILENAME_LENGTH + 1], unsigned int *sizes, int count)
{
  int i;
  int j;
  unsigned int tmp_size;
  char tmp_name[BRFS_MAX_FILENAME_LENGTH + 1];

  for (i = 0; i < count; i++)
  {
    for (j = i + 1; j < count; j++)
    {
      if (strcmp(names[i], names[j]) > 0)
      {
        strcpy(tmp_name, names[i]);
        strcpy(names[i], names[j]);
        strcpy(names[j], tmp_name);

        tmp_size = sizes[i];
        sizes[i] = sizes[j];
        sizes[j] = tmp_size;
      }
    }
  }
}
