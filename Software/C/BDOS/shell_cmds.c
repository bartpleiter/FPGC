//
// BDOS shell command module.
//

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

// ---- Program runner globals ----
// These are used by the inline assembly in bdos_shell_cmd_run to
// save/restore BDOS state across user program execution.
unsigned int bdos_run_entry = 0;    // Entry address (start of slot)
unsigned int bdos_run_stack = 0;    // Stack top for user program
unsigned int bdos_run_saved_sp = 0; // Saved BDOS stack pointer (r13)
unsigned int bdos_run_saved_bp = 0; // Saved BDOS base pointer (r14)
int bdos_run_retval = 0;           // Return value from user program

// ---- Utility helpers ----

// Trim leading and trailing whitespace in-place.
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

// Split line into argc/argv tokens. Returns -1 if too many arguments.
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

// Parse "yes"/"no" style input. Returns 0 on success, -1 on invalid input.
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

// Prepend cwd to a relative path.
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

// Resolve . and .. components in an absolute path.
int bdos_shell_normalize_path(char* input_path, char* out_path)
{
  char token[BRFS_MAX_FILENAME_LENGTH + 1];
  char components[32][BRFS_MAX_FILENAME_LENGTH + 1];
  int comp_count;
  int in_i;
  int token_len;
  int out_i;
  int j;
  int k;

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

// Resolve a possibly relative path to a normalized absolute path.
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

// Check filesystem is mounted, print error if not.
int bdos_shell_require_fs_ready()
{
  if (!bdos_fs_ready)
  {
    term_puts("error: filesystem not mounted\n");
    return 0;
  }

  return 1;
}

// Print a filesystem error with context.
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

// Convert unsigned int to decimal string, return length.
int bdos_shell_u32_to_str(unsigned int value, char* out)
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

void bdos_shell_print_hline(unsigned int length)
{
  unsigned int i;

  for (i = 0; i < length; i++)
  {
    term_putchar('-');
  }
  term_putchar('\n');
}

void bdos_shell_print_field_prefix(char* name, int value_col)
{
  int len;
  int i;

  term_puts(name);
  len = strlen(name);
  for (i = len; i < value_col; i++)
  {
    term_putchar(' ');
  }
}

// Format a word count as human-readable size string.
int bdos_shell_format_word_size(unsigned int words, char* out)
{
  int len;
  unsigned int kiw_1dp;

  if (words >= 1024)
  {
    kiw_1dp = bdos_shell_words_to_kiw_1dp(words);
    len = 0;
    len += bdos_shell_u32_to_str(kiw_1dp / 10, out + len);
    out[len++] = '.';
    out[len++] = (char)('0' + (kiw_1dp % 10));
    out[len++] = ' ';
    out[len++] = 'K';
    out[len++] = 'i';
    out[len++] = 'W';
    out[len] = '\0';
    return len;
  }

  len = bdos_shell_u32_to_str(words, out);
  out[len++] = ' ';
  out[len++] = 'W';
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

void bdos_shell_sort_files(char names[][BRFS_MAX_FILENAME_LENGTH + 1], unsigned int* sizes, int count)
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

// ---- Format wizard / special modes ----

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

// Handle input lines during format wizard or boot format confirmation.
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

// Run on shell startup to handle failed mount scenarios.
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

// ---- Built-in commands ----

int bdos_shell_cmd_help(int argc, char** argv)
{
  (void)argc;
  (void)argv;

  term_puts("BDOS shell help\n");
  term_puts("--------------\n");
  term_puts("General\n");
  term_puts("  help  clear  echo\n");
  term_puts("  uptime\n");
  term_puts("Programs\n");
  term_puts("  run <program>\n");
  term_puts("Filesystem\n");
  term_puts("  pwd  cd  ls  df\n");
  term_puts("  mkdir  mkfile  rm\n");
  term_puts("  cat  write\n");
  term_puts("Maintenance\n");
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
  char dir_names[BDOS_SHELL_LS_MAX_ENTRIES][BRFS_MAX_FILENAME_LENGTH + 1];
  char file_names[BDOS_SHELL_LS_MAX_ENTRIES][BRFS_MAX_FILENAME_LENGTH + 1];
  unsigned int file_sizes[BDOS_SHELL_LS_MAX_ENTRIES];
  char resolved[BDOS_SHELL_PATH_MAX];
  char size_buf[20];
  char name[BRFS_MAX_FILENAME_LENGTH + 1];
  int result;
  int count;
  int i;
  int dir_count;
  int file_count;
  int prefix_len;
  int size_len;
  int size_col;
  int spaces;

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

  dir_count = 0;
  file_count = 0;
  size_col = 20;

  for (i = 0; i < count; i++)
  {
    brfs_decompress_string(name, entries[i].filename, 4);

    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
    {
      continue;
    }

    if (entries[i].flags & BRFS_FLAG_DIRECTORY)
    {
      if (dir_count < BDOS_SHELL_LS_MAX_ENTRIES)
      {
        strcpy(dir_names[dir_count], name);
        dir_count++;
      }
    }
    else
    {
      if (file_count < BDOS_SHELL_LS_MAX_ENTRIES)
      {
        strcpy(file_names[file_count], name);
        file_sizes[file_count] = entries[i].filesize;
        file_count++;
      }
    }
  }

  bdos_shell_sort_names(dir_names, dir_count);
  bdos_shell_sort_files(file_names, file_sizes, file_count);

  for (i = 0; i < dir_count; i++)
  {
    term_puts(dir_names[i]);
    term_putchar('\n');
  }

  for (i = 0; i < file_count; i++)
  {
    term_puts(file_names[i]);

    size_len = bdos_shell_format_word_size(file_sizes[i], size_buf);
    prefix_len = 2 + strlen(file_names[i]);
    spaces = size_col - prefix_len;
    if (spaces < 1)
    {
      spaces = 1;
    }

    while (spaces > 0)
    {
      term_putchar(' ');
      spaces--;
    }

    term_puts(size_buf);
    term_putchar('\n');
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

int bdos_shell_cmd_mkfile(int argc, char** argv)
{
  char resolved[BDOS_SHELL_PATH_MAX];
  int result;

  if (!bdos_shell_require_fs_ready())
  {
    return 0;
  }

  if (argc != 2)
  {
    term_puts("usage: mkfile <path>\n");
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
    bdos_shell_print_fs_error("mkfile", result);
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

// Print file contents to terminal.
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

// Write text arguments into a file (replacing if it exists).
int bdos_shell_cmd_write(int argc, char** argv)
{
  char resolved[BDOS_SHELL_PATH_MAX];
  unsigned int words[BDOS_SHELL_INPUT_MAX];
  int result;
  int fd;
  int i;
  int j;
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

// Show filesystem usage statistics.
int bdos_shell_cmd_df(int argc, char** argv)
{
  unsigned int total_blocks;
  unsigned int free_blocks;
  unsigned int words_per_block;
  unsigned int used_blocks;
  unsigned int total_words;
  unsigned int used_words;
  unsigned int usage_percent;
  char label[11];
  char line_header[20];
  char value_buf[32];
  int result_label;
  int line_len;
  int value_col;
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
  usage_percent = (total_blocks == 0) ? 0 : ((used_blocks * 100) / total_blocks);

  result_label = brfs_get_label(label, sizeof(label));
  if (result_label != BRFS_OK || strlen(label) == 0)
  {
    strcpy(label, "(unnamed)");
  }

  strcpy(line_header, "Label: ");
  strcat(line_header, label);
  line_len = strlen(line_header);
  value_col = 14;

  term_puts(line_header);
  term_putchar('\n');
  bdos_shell_print_hline((unsigned int)line_len);

  bdos_shell_print_field_prefix("Total:", value_col);
  bdos_shell_print_kiw(total_words);
  term_putchar('\n');

  bdos_shell_print_field_prefix("Used:", value_col);
  bdos_shell_print_kiw(used_words);
  term_puts(" (");
  term_putint((int)usage_percent);
  term_puts("%)\n");

  bdos_shell_print_field_prefix("Blocks:", value_col);
  bdos_shell_u32_to_str(used_blocks, value_buf);
  term_puts(value_buf);
  term_putchar('/');
  bdos_shell_u32_to_str(total_blocks, value_buf);
  term_puts(value_buf);
  term_puts(" used\n");

  bdos_shell_print_field_prefix("Block size:", value_col);
  term_putint((int)words_per_block);
  term_puts(" W\n");

  return 0;
}

// Load a binary from BRFS into the first user program slot and execute it.
// If the path has no directory component, tries /bin/<name> as fallback.
// The program runs to completion; BDOS regains control when main() returns.
int bdos_shell_cmd_run(int argc, char** argv)
{
  char resolved[BDOS_SHELL_PATH_MAX];
  char bin_path[BDOS_SHELL_PATH_MAX];
  int result;
  int fd;
  int file_size;
  int words_remaining;
  int chunk_len;
  int words_read;
  unsigned int *dest;

  if (!bdos_shell_require_fs_ready())
  {
    return 0;
  }

  if (argc < 2)
  {
    term_puts("usage: run <program>\n");
    term_puts("  Loads and runs a binary from the filesystem.\n");
    term_puts("  If no path separator, looks in /bin/ directory.\n");
    return 0;
  }

  // Try to resolve the path as given
  result = bdos_shell_resolve_path(argv[1], resolved);
  if (result != BRFS_OK)
  {
    // If the name has no slash, try /bin/<name>
    if (strchr(argv[1], '/') == 0)
    {
      strcpy(bin_path, "/bin/");
      strcat(bin_path, argv[1]);

      result = bdos_shell_resolve_path(bin_path, resolved);
      if (result != BRFS_OK)
      {
        bdos_shell_print_fs_error("resolve path", result);
        return 0;
      }
    }
    else
    {
      bdos_shell_print_fs_error("resolve path", result);
      return 0;
    }
  }

  // Open the binary file
  fd = brfs_open(resolved);
  if (fd < 0)
  {
    // If direct open failed and name has no slash, try /bin/
    if (strchr(argv[1], '/') == 0)
    {
      strcpy(bin_path, "/bin/");
      strcat(bin_path, argv[1]);
      result = bdos_shell_resolve_path(bin_path, resolved);
      if (result == BRFS_OK)
      {
        fd = brfs_open(resolved);
      }
    }

    if (fd < 0)
    {
      bdos_shell_print_fs_error("open", fd);
      return 0;
    }
  }

  file_size = brfs_file_size(fd);
  if (file_size <= 0)
  {
    term_puts("error: empty or invalid binary\n");
    brfs_close(fd);
    return 0;
  }

  // Check if the binary fits in one slot (512 KiW)
  if ((unsigned int)file_size > MEM_SLOT_SIZE)
  {
    term_puts("error: binary too large for one slot (");
    term_putint(file_size);
    term_puts(" words, max ");
    term_putint((int)MEM_SLOT_SIZE);
    term_puts(")\n");
    brfs_close(fd);
    return 0;
  }

  // Load binary into slot 0 (MEM_PROGRAM_START)
  term_puts("Loading ");
  term_puts(resolved);
  term_puts(" (");
  term_putint(file_size);
  term_puts(" words)...\n");

  dest = (unsigned int *)MEM_PROGRAM_START;
  words_remaining = file_size;

  while (words_remaining > 0)
  {
    chunk_len = words_remaining;
    if (chunk_len > 256)
    {
      chunk_len = 256;
    }

    words_read = brfs_read(fd, dest, (unsigned int)chunk_len);
    if (words_read < 0)
    {
      bdos_shell_print_fs_error("read", words_read);
      brfs_close(fd);
      return 0;
    }

    if (words_read == 0)
    {
      break;
    }

    dest = dest + words_read;
    words_remaining = words_remaining - words_read;
  }

  brfs_close(fd);

  // Flush the instruction cache since we just wrote new code to RAM
  asm("ccache");

  // Set up globals for the inline assembly trampoline
  bdos_run_entry = MEM_PROGRAM_START;
  bdos_run_stack = MEM_PROGRAM_START + MEM_SLOT_SIZE - 1;

  term_puts("Running...\n");

  // Execute the user program via inline assembly.
  //
  // We save all BDOS registers to the stack (except r13/r14 which go
  // to globals, since the user program gets its own stack).
  // The program's entry point at slot offset 0 is the ASMPY header
  // "jump Main".  When the user's main() returns, r15 brings execution
  // back to Label_bdos_run_return.  Return value is in r1.
  asm(
      "push r1"
      "push r2"
      "push r3"
      "push r4"
      "push r5"
      "push r6"
      "push r7"
      "push r8"
      "push r9"
      "push r10"
      "push r11"
      "push r12"
      "push r15"
      "addr2reg Label_bdos_run_saved_sp r11"
      "write 0 r11 r13                       ; save BDOS stack pointer"
      "addr2reg Label_bdos_run_saved_bp r11"
      "write 0 r11 r14                       ; save BDOS base pointer"
      "addr2reg Label_bdos_run_entry r11"
      "read 0 r11 r11                        ; r11 = user program entry"
      "addr2reg Label_bdos_run_stack r12"
      "read 0 r12 r13                        ; r13 = user program stack"
      "addr2reg Label_bdos_run_return r15     ; r15 = return-to-BDOS address"
      "jumpr 0 r11                            ; jump to user program"
      "Label_bdos_run_return:"
      "addr2reg Label_bdos_run_retval r11"
      "write 0 r11 r1                         ; save user return value"
      "addr2reg Label_bdos_run_saved_sp r11"
      "read 0 r11 r13                         ; restore BDOS stack pointer"
      "addr2reg Label_bdos_run_saved_bp r11"
      "read 0 r11 r14                         ; restore BDOS base pointer"
      "pop r15"
      "pop r12"
      "pop r11"
      "pop r10"
      "pop r9"
      "pop r8"
      "pop r7"
      "pop r6"
      "pop r5"
      "pop r4"
      "pop r3"
      "pop r2"
      "pop r1"
  );

  // Flush cache again after user program execution
  asm("ccache");

  term_puts("Program exited with code ");
  term_putint(bdos_run_retval);
  term_putchar('\n');

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

// ---- Command dispatcher ----

// Dispatch a parsed command line to the appropriate handler.
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

  if (strcmp(argv[0], "mkfile") == 0)
  {
    bdos_shell_cmd_mkfile(argc, argv);
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

  if (strcmp(argv[0], "run") == 0)
  {
    bdos_shell_cmd_run(argc, argv);
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
