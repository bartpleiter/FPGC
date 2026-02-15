/*
 * BDOS shell command module.
 */

#include "BDOS/bdos.h"

#define BDOS_SHELL_LS_MAX_ENTRIES 32
#define BDOS_SHELL_IO_CHUNK_WORDS 64
#define BDOS_SHELL_FORMAT_LABEL_MAX 10

#define BDOS_SHELL_MODE_NORMAL            0
#define BDOS_SHELL_MODE_BOOT_FORMAT_YN    1
#define BDOS_SHELL_MODE_FORMAT_BLOCKS     2
#define BDOS_SHELL_MODE_FORMAT_WORDS      3
#define BDOS_SHELL_MODE_FORMAT_LABEL      4
#define BDOS_SHELL_MODE_FORMAT_FULL       5

int bdos_shell_mode = BDOS_SHELL_MODE_NORMAL;
unsigned int bdos_shell_format_blocks = 0;
unsigned int bdos_shell_format_words = 0;
char bdos_shell_format_label[BDOS_SHELL_FORMAT_LABEL_MAX + 1];
int bdos_shell_format_full = 0;

/* ------------------------------------------------------------------------- */
/* Utility helpers                                                            */
/* ------------------------------------------------------------------------- */

void bdos_shell_trim_whitespace(char* s)
{
  int len;
  int start;
  int i;

  len = strlen(s);
  start = 0;

  while (s[start] == ' ' || s[start] == '\t')
  {
    start++;
  }

  while (len > start && (s[len - 1] == ' ' || s[len - 1] == '\t'))
  {
    len--;
  }

  if (start > 0)
  {
    for (i = 0; i < (len - start); i++)
    {
      s[i] = s[start + i];
    }
  }

  s[len - start] = '\0';
}

int bdos_shell_parse_line(char* line, int* argc_out, char** argv)
{
  int argc;
  char* p;

  argc = 0;
  p = line;

  while (*p != '\0')
  {
    while (*p == ' ' || *p == '\t')
    {
      p++;
    }

    if (*p == '\0')
    {
      break;
    }

    if (argc >= BDOS_SHELL_ARGV_MAX)
    {
      return -1;
    }

    argv[argc] = p;
    argc++;

    while (*p != '\0' && *p != ' ' && *p != '\t')
    {
      p++;
    }

    if (*p == '\0')
    {
      break;
    }

    *p = '\0';
    p++;
  }

  *argc_out = argc;
  return 0;
}

int bdos_shell_parse_yes_no(char* value, int* out_yes)
{
  if (strcmp(value, "y") == 0 || strcmp(value, "yes") == 0 || strcmp(value, "1") == 0)
  {
    *out_yes = 1;
    return 0;
  }

  if (strcmp(value, "n") == 0 || strcmp(value, "no") == 0 || strcmp(value, "0") == 0)
  {
    *out_yes = 0;
    return 0;
  }

  return -1;
}

int bdos_shell_path_is_absolute(char* path)
{
  return (path[0] == '/');
}

int bdos_shell_build_absolute_path(char* input_path, char* out_path)
{
  int in_len;
  int cwd_len;

  if (input_path == NULL || out_path == NULL)
  {
    return BRFS_ERR_INVALID_PARAM;
  }

  if (bdos_shell_path_is_absolute(input_path))
  {
    in_len = strlen(input_path);
    if (in_len >= BDOS_SHELL_PATH_MAX)
    {
      return BRFS_ERR_PATH_TOO_LONG;
    }
    strcpy(out_path, input_path);
    return BRFS_OK;
  }

  cwd_len = strlen(bdos_shell_cwd);
  in_len = strlen(input_path);

  if (cwd_len == 1 && bdos_shell_cwd[0] == '/')
  {
    if ((1 + in_len) >= BDOS_SHELL_PATH_MAX)
    {
      return BRFS_ERR_PATH_TOO_LONG;
    }

    out_path[0] = '/';
    out_path[1] = '\0';
    strcat(out_path, input_path);
    return BRFS_OK;
  }

  if ((cwd_len + 1 + in_len) >= BDOS_SHELL_PATH_MAX)
  {
    return BRFS_ERR_PATH_TOO_LONG;
  }

  strcpy(out_path, bdos_shell_cwd);
  strcat(out_path, "/");
  strcat(out_path, input_path);

  return BRFS_OK;
}

int bdos_shell_normalize_path(char* input_path, char* out_path)
{
  char token[BRFS_MAX_FILENAME_LENGTH + 1];
  char components[32][BRFS_MAX_FILENAME_LENGTH + 1];
  int comp_count;
  int in_i;
  int token_len;
  int out_i;
  int j;

  if (input_path == NULL || out_path == NULL)
  {
    return BRFS_ERR_INVALID_PARAM;
  }

  comp_count = 0;
  in_i = 0;

  if (input_path[0] == '/')
  {
    in_i = 1;
  }

  while (1)
  {
    while (input_path[in_i] == '/')
    {
      in_i++;
    }

    if (input_path[in_i] == '\0')
    {
      break;
    }

    token_len = 0;
    while (input_path[in_i] != '\0' && input_path[in_i] != '/')
    {
      if (token_len >= BRFS_MAX_FILENAME_LENGTH)
      {
        return BRFS_ERR_NAME_TOO_LONG;
      }
      token[token_len] = input_path[in_i];
      token_len++;
      in_i++;
    }
    token[token_len] = '\0';

    if (strcmp(token, ".") == 0)
    {
      continue;
    }

    if (strcmp(token, "..") == 0)
    {
      if (comp_count > 0)
      {
        comp_count--;
      }
      continue;
    }

    if (comp_count >= 32)
    {
      return BRFS_ERR_PATH_TOO_LONG;
    }

    strcpy(components[comp_count], token);
    comp_count++;
  }

  if (comp_count == 0)
  {
    out_path[0] = '/';
    out_path[1] = '\0';
    return BRFS_OK;
  }

  out_i = 0;
  out_path[out_i++] = '/';

  for (j = 0; j < comp_count; j++)
  {
    int k;

    if (j > 0)
    {
      out_path[out_i++] = '/';
    }

    for (k = 0; components[j][k] != '\0'; k++)
    {
      if (out_i >= (BDOS_SHELL_PATH_MAX - 1))
      {
        return BRFS_ERR_PATH_TOO_LONG;
      }
      out_path[out_i++] = components[j][k];
    }
  }

  out_path[out_i] = '\0';
  return BRFS_OK;
}

int bdos_shell_resolve_path(char* input_path, char* out_path)
{
  int result;
  char abs_path[BDOS_SHELL_PATH_MAX];

  result = bdos_shell_build_absolute_path(input_path, abs_path);
  if (result != BRFS_OK)
  {
    return result;
  }

  return bdos_shell_normalize_path(abs_path, out_path);
}

int bdos_shell_require_fs_ready()
{
  if (!bdos_fs_ready)
  {
    term_puts("error: filesystem not mounted\n");
    return 0;
  }

  return 1;
}

void bdos_shell_print_fs_error(char* action, int result)
{
  term_puts("error: ");
  term_puts(action);
  term_puts(" failed: ");
  term_puts(bdos_fs_error_string(result));
  term_putchar('\n');
}

void bdos_shell_print_2digit(unsigned int value)
{
  if (value < 10)
  {
    term_putchar('0');
  }
  term_putint((int)value);
}

unsigned int bdos_shell_words_to_kiw_1dp(unsigned int words)
{
  return (words * 10) / 1024;
}

void bdos_shell_print_kiw(unsigned int words)
{
  unsigned int kiw_1dp;

  kiw_1dp = bdos_shell_words_to_kiw_1dp(words);
  term_putint((int)(kiw_1dp / 10));
  term_putchar('.');
  term_putint((int)(kiw_1dp % 10));
  term_puts(" KiW");
}

/* ------------------------------------------------------------------------- */
/* Format wizard / special modes                                              */
/* ------------------------------------------------------------------------- */

void bdos_shell_start_format_wizard()
{
  bdos_shell_mode = BDOS_SHELL_MODE_FORMAT_BLOCKS;
  term_puts("Filesystem format wizard\n");
  term_puts("Enter total blocks (multiple of 64):\n");
}

void bdos_shell_finish_format_wizard()
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

int bdos_shell_handle_special_mode_line(char* line)
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
    term_puts("Enter words per block (multiple of 64):\n");
    return 1;
  }

  if (bdos_shell_mode == BDOS_SHELL_MODE_FORMAT_WORDS)
  {
    value = atoi(line);
    if (value <= 0)
    {
      term_puts("Invalid words-per-block. Please enter a positive integer.\n");
      return 1;
    }

    bdos_shell_format_words = (unsigned int)value;
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

void bdos_shell_on_startup()
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

/* ------------------------------------------------------------------------- */
/* Built-in commands                                                          */
/* ------------------------------------------------------------------------- */

int bdos_shell_cmd_help(int argc, char** argv)
{
  (void)argc;
  (void)argv;

  term_puts("Available commands\n");
  term_puts("------------------\n");
  term_puts("General:\n");
  term_puts("  help  clear  echo  uptime\n");
  term_puts("Filesystem:\n");
  term_puts("  pwd  cd  ls  df\n");
  term_puts("  mkdir  touch  rm\n");
  term_puts("  cat  write\n");
  term_puts("Maintenance:\n");
  term_puts("  format  sync\n");
  return 0;
}

int bdos_shell_cmd_clear(int argc, char** argv)
{
  (void)argc;
  (void)argv;
  term_clear();
  return 0;
}

int bdos_shell_cmd_echo(int argc, char** argv)
{
  int i;

  for (i = 1; i < argc; i++)
  {
    term_puts(argv[i]);
    if (i < argc - 1)
    {
      term_putchar(' ');
    }
  }
  term_putchar('\n');

  return 0;
}

int bdos_shell_cmd_uptime(int argc, char** argv)
{
  unsigned int elapsed_us;
  unsigned int elapsed_seconds;
  unsigned int days;
  unsigned int hours;
  unsigned int minutes;
  unsigned int seconds;

  (void)argc;
  (void)argv;

  elapsed_us = get_micros() - bdos_shell_start_micros;
  elapsed_seconds = elapsed_us / 1000000;

  days = elapsed_seconds / 86400;
  elapsed_seconds = elapsed_seconds % 86400;
  hours = elapsed_seconds / 3600;
  elapsed_seconds = elapsed_seconds % 3600;
  minutes = elapsed_seconds / 60;
  seconds = elapsed_seconds % 60;

  term_puts("Uptime: ");
  term_putint((int)days);
  term_puts("d ");
  bdos_shell_print_2digit(hours);
  term_puts("h ");
  bdos_shell_print_2digit(minutes);
  term_puts("m ");
  bdos_shell_print_2digit(seconds);
  term_puts("s\n");
  return 0;
}

int bdos_shell_cmd_pwd(int argc, char** argv)
{
  (void)argc;
  (void)argv;
  term_puts(bdos_shell_cwd);
  term_putchar('\n');
  return 0;
}

int bdos_shell_cmd_cd(int argc, char** argv)
{
  char resolved[BDOS_SHELL_PATH_MAX];
  int result;

  if (!bdos_shell_require_fs_ready())
  {
    return 0;
  }

  if (argc != 2)
  {
    term_puts("usage: cd <path>\n");
    return 0;
  }

  result = bdos_shell_resolve_path(argv[1], resolved);
  if (result != BRFS_OK)
  {
    bdos_shell_print_fs_error("resolve path", result);
    return 0;
  }

  if (!brfs_is_dir(resolved))
  {
    term_puts("error: not a directory\n");
    return 0;
  }

  strcpy(bdos_shell_cwd, resolved);
  return 0;
}

int bdos_shell_cmd_ls(int argc, char** argv)
{
  struct brfs_dir_entry entries[BDOS_SHELL_LS_MAX_ENTRIES];
  char resolved[BDOS_SHELL_PATH_MAX];
  char name[BRFS_MAX_FILENAME_LENGTH + 1];
  int result;
  int count;
  int i;

  if (!bdos_shell_require_fs_ready())
  {
    return 0;
  }

  if (argc > 2)
  {
    term_puts("usage: ls [path]\n");
    return 0;
  }

  if (argc == 2)
  {
    result = bdos_shell_resolve_path(argv[1], resolved);
  }
  else
  {
    strcpy(resolved, bdos_shell_cwd);
    result = BRFS_OK;
  }

  if (result != BRFS_OK)
  {
    bdos_shell_print_fs_error("resolve path", result);
    return 0;
  }

  count = brfs_read_dir(resolved, entries, BDOS_SHELL_LS_MAX_ENTRIES);
  if (count < 0)
  {
    bdos_shell_print_fs_error("ls", count);
    return 0;
  }

  for (i = 0; i < count; i++)
  {
    brfs_decompress_string(name, entries[i].filename, 4);

    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
    {
      continue;
    }

    if (entries[i].flags & BRFS_FLAG_DIRECTORY)
    {
      term_puts("[DIR]  ");
      term_puts(name);
      term_putchar('\n');
    }
    else
    {
      term_puts("[FILE] ");
      term_puts(name);
      term_puts(" (");
      term_putint((int)entries[i].filesize);
      term_puts(" words)\n");
    }
  }

  return 0;
}

int bdos_shell_cmd_mkdir(int argc, char** argv)
{
  char resolved[BDOS_SHELL_PATH_MAX];
  int result;

  if (!bdos_shell_require_fs_ready())
  {
    return 0;
  }

  if (argc != 2)
  {
    term_puts("usage: mkdir <path>\n");
    return 0;
  }

  result = bdos_shell_resolve_path(argv[1], resolved);
  if (result != BRFS_OK)
  {
    bdos_shell_print_fs_error("resolve path", result);
    return 0;
  }

  result = brfs_create_dir(resolved);
  if (result != BRFS_OK)
  {
    bdos_shell_print_fs_error("mkdir", result);
  }

  return 0;
}

int bdos_shell_cmd_touch(int argc, char** argv)
{
  char resolved[BDOS_SHELL_PATH_MAX];
  int result;

  if (!bdos_shell_require_fs_ready())
  {
    return 0;
  }

  if (argc != 2)
  {
    term_puts("usage: touch <path>\n");
    return 0;
  }

  result = bdos_shell_resolve_path(argv[1], resolved);
  if (result != BRFS_OK)
  {
    bdos_shell_print_fs_error("resolve path", result);
    return 0;
  }

  result = brfs_create_file(resolved);
  if (result != BRFS_OK)
  {
    bdos_shell_print_fs_error("touch", result);
  }

  return 0;
}

int bdos_shell_cmd_rm(int argc, char** argv)
{
  char resolved[BDOS_SHELL_PATH_MAX];
  int result;

  if (!bdos_shell_require_fs_ready())
  {
    return 0;
  }

  if (argc != 2)
  {
    term_puts("usage: rm <path>\n");
    return 0;
  }

  result = bdos_shell_resolve_path(argv[1], resolved);
  if (result != BRFS_OK)
  {
    bdos_shell_print_fs_error("resolve path", result);
    return 0;
  }

  result = brfs_delete(resolved);
  if (result != BRFS_OK)
  {
    bdos_shell_print_fs_error("rm", result);
  }

  return 0;
}

int bdos_shell_cmd_cat(int argc, char** argv)
{
  char resolved[BDOS_SHELL_PATH_MAX];
  unsigned int chunk[BDOS_SHELL_IO_CHUNK_WORDS];
  int result;
  int fd;
  int remaining;
  int chunk_len;
  int words_read;
  int i;
  char c;

  if (!bdos_shell_require_fs_ready())
  {
    return 0;
  }

  if (argc != 2)
  {
    term_puts("usage: cat <path>\n");
    return 0;
  }

  result = bdos_shell_resolve_path(argv[1], resolved);
  if (result != BRFS_OK)
  {
    bdos_shell_print_fs_error("resolve path", result);
    return 0;
  }

  fd = brfs_open(resolved);
  if (fd < 0)
  {
    bdos_shell_print_fs_error("open", fd);
    return 0;
  }

  remaining = brfs_file_size(fd);
  while (remaining > 0)
  {
    chunk_len = remaining;
    if (chunk_len > BDOS_SHELL_IO_CHUNK_WORDS)
    {
      chunk_len = BDOS_SHELL_IO_CHUNK_WORDS;
    }

    words_read = brfs_read(fd, chunk, (unsigned int)chunk_len);
    if (words_read < 0)
    {
      bdos_shell_print_fs_error("read", words_read);
      brfs_close(fd);
      return 0;
    }

    for (i = 0; i < words_read; i++)
    {
      c = (char)(chunk[i] & 0xFF);
      if (c == '\n' || c == '\r' || c == '\t' || (c >= 32 && c <= 126))
      {
        term_putchar(c);
      }
      else
      {
        term_putchar('.');
      }
    }

    remaining -= words_read;

    if (words_read == 0)
    {
      break;
    }
  }

  term_putchar('\n');
  brfs_close(fd);
  return 0;
}

int bdos_shell_cmd_write(int argc, char** argv)
{
  char resolved[BDOS_SHELL_PATH_MAX];
  unsigned int words[BDOS_SHELL_INPUT_MAX];
  int result;
  int fd;
  int i;
  int write_index;

  if (!bdos_shell_require_fs_ready())
  {
    return 0;
  }

  if (argc < 3)
  {
    term_puts("usage: write <path> <text>\n");
    return 0;
  }

  result = bdos_shell_resolve_path(argv[1], resolved);
  if (result != BRFS_OK)
  {
    bdos_shell_print_fs_error("resolve path", result);
    return 0;
  }

  if (brfs_exists(resolved))
  {
    if (brfs_is_dir(resolved))
    {
      term_puts("error: cannot write to directory\n");
      return 0;
    }

    result = brfs_delete(resolved);
    if (result != BRFS_OK)
    {
      bdos_shell_print_fs_error("replace file", result);
      return 0;
    }
  }

  result = brfs_create_file(resolved);
  if (result != BRFS_OK)
  {
    bdos_shell_print_fs_error("create file", result);
    return 0;
  }

  fd = brfs_open(resolved);
  if (fd < 0)
  {
    bdos_shell_print_fs_error("open", fd);
    return 0;
  }

  write_index = 0;
  for (i = 2; i < argc; i++)
  {
    int j;

    if (i > 2)
    {
      if (write_index >= BDOS_SHELL_INPUT_MAX)
      {
        term_puts("error: text too long\n");
        brfs_close(fd);
        return 0;
      }
      words[write_index++] = (unsigned int)' ';
    }

    for (j = 0; argv[i][j] != '\0'; j++)
    {
      if (write_index >= BDOS_SHELL_INPUT_MAX)
      {
        term_puts("error: text too long\n");
        brfs_close(fd);
        return 0;
      }
      words[write_index++] = (unsigned int)(unsigned char)argv[i][j];
    }
  }

  result = brfs_write(fd, words, (unsigned int)write_index);
  if (result < 0)
  {
    bdos_shell_print_fs_error("write", result);
    brfs_close(fd);
    return 0;
  }

  brfs_close(fd);

  term_puts("wrote ");
  term_putint(write_index);
  term_puts(" words\n");
  return 0;
}

int bdos_shell_cmd_sync(int argc, char** argv)
{
  int result;

  (void)argc;
  (void)argv;

  result = bdos_fs_sync_now();
  if (result != BRFS_OK)
  {
    bdos_shell_print_fs_error("sync", result);
    return 0;
  }
  return 0;
}

int bdos_shell_cmd_df(int argc, char** argv)
{
  unsigned int total_blocks;
  unsigned int free_blocks;
  unsigned int words_per_block;
  unsigned int used_blocks;
  unsigned int total_words;
  unsigned int used_words;
  unsigned int free_words;
  unsigned int usage_percent;
  int result;

  (void)argc;
  (void)argv;

  if (!bdos_shell_require_fs_ready())
  {
    return 0;
  }

  result = brfs_statfs(&total_blocks, &free_blocks, &words_per_block);
  if (result != BRFS_OK)
  {
    bdos_shell_print_fs_error("df", result);
    return 0;
  }

  used_blocks = total_blocks - free_blocks;
  total_words = total_blocks * words_per_block;
  used_words = used_blocks * words_per_block;
  free_words = free_blocks * words_per_block;
  usage_percent = (total_blocks == 0) ? 0 : ((used_blocks * 100) / total_blocks);

  term_puts("Filesystem usage\n");
  term_puts("----------------\n");
  term_puts("Total: ");
  bdos_shell_print_kiw(total_words);
  term_putchar('\n');
  term_puts("Used : ");
  bdos_shell_print_kiw(used_words);
  term_puts(" (");
  term_putint((int)usage_percent);
  term_puts("%)\n");
  term_puts("Free : ");
  bdos_shell_print_kiw(free_words);
  term_putchar('\n');
  term_puts("Blocks: ");
  term_putint((int)used_blocks);
  term_puts("/");
  term_putint((int)total_blocks);
  term_puts(" used\n");
  term_puts("Free blocks: ");
  term_putint((int)free_blocks);
  term_putchar('\n');
  term_puts("Block size: ");
  term_putint((int)words_per_block);
  term_puts(" W\n");

  return 0;
}

int bdos_shell_cmd_format(int argc, char** argv)
{
  if (argc != 1)
  {
    term_puts("usage: format\n");
    return 0;
  }

  bdos_shell_start_format_wizard();
  return 0;
}

/* ------------------------------------------------------------------------- */
/* Command dispatcher                                                         */
/* ------------------------------------------------------------------------- */

void bdos_shell_execute_line(char* line)
{
  int argc;
  char* argv[BDOS_SHELL_ARGV_MAX];

  if (bdos_shell_parse_line(line, &argc, argv) != 0)
  {
    term_puts("error: too many arguments\n");
    return;
  }

  if (argc == 0)
  {
    return;
  }

  if (strcmp(argv[0], "help") == 0)
  {
    bdos_shell_cmd_help(argc, argv);
    return;
  }

  if (strcmp(argv[0], "clear") == 0)
  {
    bdos_shell_cmd_clear(argc, argv);
    return;
  }

  if (strcmp(argv[0], "echo") == 0)
  {
    bdos_shell_cmd_echo(argc, argv);
    return;
  }

  if (strcmp(argv[0], "uptime") == 0)
  {
    bdos_shell_cmd_uptime(argc, argv);
    return;
  }

  if (strcmp(argv[0], "pwd") == 0)
  {
    bdos_shell_cmd_pwd(argc, argv);
    return;
  }

  if (strcmp(argv[0], "cd") == 0)
  {
    bdos_shell_cmd_cd(argc, argv);
    return;
  }

  if (strcmp(argv[0], "ls") == 0)
  {
    bdos_shell_cmd_ls(argc, argv);
    return;
  }

  if (strcmp(argv[0], "mkdir") == 0)
  {
    bdos_shell_cmd_mkdir(argc, argv);
    return;
  }

  if (strcmp(argv[0], "touch") == 0)
  {
    bdos_shell_cmd_touch(argc, argv);
    return;
  }

  if (strcmp(argv[0], "rm") == 0)
  {
    bdos_shell_cmd_rm(argc, argv);
    return;
  }

  if (strcmp(argv[0], "cat") == 0)
  {
    bdos_shell_cmd_cat(argc, argv);
    return;
  }

  if (strcmp(argv[0], "write") == 0)
  {
    bdos_shell_cmd_write(argc, argv);
    return;
  }

  if (strcmp(argv[0], "format") == 0)
  {
    bdos_shell_cmd_format(argc, argv);
    return;
  }

  if (strcmp(argv[0], "sync") == 0)
  {
    bdos_shell_cmd_sync(argc, argv);
    return;
  }

  if (strcmp(argv[0], "df") == 0)
  {
    bdos_shell_cmd_df(argc, argv);
    return;
  }

  term_puts("error: unknown command: ");
  term_puts(argv[0]);
  term_puts("\nType 'help' to list commands.\n");
}
