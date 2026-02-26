//
// BDOS shell built-in commands and command dispatcher.
//

#include "BDOS/bdos.h"

#define BDOS_SHELL_LS_MAX_ENTRIES 32
#define BDOS_SHELL_IO_CHUNK_WORDS 64

// Program argc/argv storage (accessible to user programs via syscall)
int bdos_shell_prog_argc = 0;
char* bdos_shell_prog_argv[BDOS_SHELL_ARGV_MAX];

// ---- Built-in commands ----

int bdos_shell_cmd_help(int argc, char** argv)
{
  (void)argc;
  (void)argv;

  term_puts("BDOS shell help\n");
  term_puts("--------------\n");
  term_puts("General\n");
  term_puts("  help  clear  echo  uptime\n");
  term_puts("Filesystem\n");
  term_puts("  pwd  cd  ls  df\n");
  term_puts("  mkdir  mkfile  rm\n");
  term_puts("  cat  write  cp  mv\n");
  term_puts("Maintenance\n");
  term_puts("  format  sync\n");
  term_puts("Programs\n");
  term_puts("  path, or filename from /bin\n");
  term_puts("  jobs  fg <slot>  kill <slot>\n");
  term_puts("Hotkeys (during program)\n");
  term_puts("  F1=shell  Alt+F4=kill\n");
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

// Copy a file.
// If dest is a directory, appends source basename: cp /a/foo /b/ -> /b/foo
int bdos_shell_cmd_cp(int argc, char** argv)
{
  char src_resolved[BDOS_SHELL_PATH_MAX];
  char dst_resolved[BDOS_SHELL_PATH_MAX];
  unsigned int chunk[BDOS_SHELL_IO_CHUNK_WORDS];
  int result;
  int src_fd;
  int dst_fd;
  int remaining;
  int chunk_len;
  int words_read;
  int words_written;
  char* slash;

  if (!bdos_shell_require_fs_ready())
  {
    return 0;
  }

  if (argc != 3)
  {
    term_puts("usage: cp <source> <dest>\n");
    return 0;
  }

  result = bdos_shell_resolve_path(argv[1], src_resolved);
  if (result != BRFS_OK)
  {
    bdos_shell_print_fs_error("resolve source", result);
    return 0;
  }

  if (!brfs_exists(src_resolved))
  {
    term_puts("cp: source not found\n");
    return 0;
  }

  if (brfs_is_dir(src_resolved))
  {
    term_puts("cp: cannot copy a directory\n");
    return 0;
  }

  result = bdos_shell_resolve_path(argv[2], dst_resolved);
  if (result != BRFS_OK)
  {
    bdos_shell_print_fs_error("resolve dest", result);
    return 0;
  }

  // If dest is an existing directory, append source basename
  if (brfs_exists(dst_resolved) && brfs_is_dir(dst_resolved))
  {
    slash = strrchr(src_resolved, '/');
    if (slash != 0)
    {
      strcat(dst_resolved, "/");
      strcat(dst_resolved, slash + 1);
    }
    else
    {
      strcat(dst_resolved, "/");
      strcat(dst_resolved, src_resolved);
    }
  }

  // If dest file already exists, delete it first
  if (brfs_exists(dst_resolved) && !brfs_is_dir(dst_resolved))
  {
    result = brfs_delete(dst_resolved);
    if (result != BRFS_OK)
    {
      bdos_shell_print_fs_error("cp: replace dest", result);
      return 0;
    }
  }

  // Create dest file
  result = brfs_create_file(dst_resolved);
  if (result != BRFS_OK)
  {
    bdos_shell_print_fs_error("cp: create dest", result);
    return 0;
  }

  // Open source
  src_fd = brfs_open(src_resolved);
  if (src_fd < 0)
  {
    bdos_shell_print_fs_error("cp: open source", src_fd);
    return 0;
  }

  // Open dest
  dst_fd = brfs_open(dst_resolved);
  if (dst_fd < 0)
  {
    bdos_shell_print_fs_error("cp: open dest", dst_fd);
    brfs_close(src_fd);
    return 0;
  }

  // Copy data in chunks
  remaining = brfs_file_size(src_fd);
  while (remaining > 0)
  {
    chunk_len = remaining;
    if (chunk_len > BDOS_SHELL_IO_CHUNK_WORDS)
    {
      chunk_len = BDOS_SHELL_IO_CHUNK_WORDS;
    }

    words_read = brfs_read(src_fd, chunk, (unsigned int)chunk_len);
    if (words_read <= 0)
    {
      if (words_read < 0)
      {
        bdos_shell_print_fs_error("cp: read", words_read);
      }
      break;
    }

    words_written = brfs_write(dst_fd, chunk, (unsigned int)words_read);
    if (words_written < 0)
    {
      bdos_shell_print_fs_error("cp: write", words_written);
      break;
    }

    remaining -= words_read;
  }

  brfs_close(src_fd);
  brfs_close(dst_fd);
  return 0;
}

// Move (rename) a file. Implemented as copy + delete
// If dest is a directory, appends source basename: mv /a/foo /b/ -> /b/foo
int bdos_shell_cmd_mv(int argc, char** argv)
{
  char src_resolved[BDOS_SHELL_PATH_MAX];
  char dst_resolved[BDOS_SHELL_PATH_MAX];
  unsigned int chunk[BDOS_SHELL_IO_CHUNK_WORDS];
  int result;
  int src_fd;
  int dst_fd;
  int remaining;
  int chunk_len;
  int words_read;
  int words_written;
  char* slash;

  if (!bdos_shell_require_fs_ready())
  {
    return 0;
  }

  if (argc != 3)
  {
    term_puts("usage: mv <source> <dest>\n");
    return 0;
  }

  result = bdos_shell_resolve_path(argv[1], src_resolved);
  if (result != BRFS_OK)
  {
    bdos_shell_print_fs_error("resolve source", result);
    return 0;
  }

  if (!brfs_exists(src_resolved))
  {
    term_puts("mv: source not found\n");
    return 0;
  }

  if (brfs_is_dir(src_resolved))
  {
    term_puts("mv: cannot move a directory\n");
    return 0;
  }

  result = bdos_shell_resolve_path(argv[2], dst_resolved);
  if (result != BRFS_OK)
  {
    bdos_shell_print_fs_error("resolve dest", result);
    return 0;
  }

  // If dest is an existing directory, append source basename
  if (brfs_exists(dst_resolved) && brfs_is_dir(dst_resolved))
  {
    slash = strrchr(src_resolved, '/');
    if (slash != 0)
    {
      strcat(dst_resolved, "/");
      strcat(dst_resolved, slash + 1);
    }
    else
    {
      strcat(dst_resolved, "/");
      strcat(dst_resolved, src_resolved);
    }
  }

  // If dest file already exists, delete it first
  if (brfs_exists(dst_resolved) && !brfs_is_dir(dst_resolved))
  {
    result = brfs_delete(dst_resolved);
    if (result != BRFS_OK)
    {
      bdos_shell_print_fs_error("mv: replace dest", result);
      return 0;
    }
  }

  // Create dest file
  result = brfs_create_file(dst_resolved);
  if (result != BRFS_OK)
  {
    bdos_shell_print_fs_error("mv: create dest", result);
    return 0;
  }

  // Open source
  src_fd = brfs_open(src_resolved);
  if (src_fd < 0)
  {
    bdos_shell_print_fs_error("mv: open source", src_fd);
    return 0;
  }

  // Open dest
  dst_fd = brfs_open(dst_resolved);
  if (dst_fd < 0)
  {
    bdos_shell_print_fs_error("mv: open dest", dst_fd);
    brfs_close(src_fd);
    return 0;
  }

  // Copy data in chunks
  remaining = brfs_file_size(src_fd);
  while (remaining > 0)
  {
    chunk_len = remaining;
    if (chunk_len > BDOS_SHELL_IO_CHUNK_WORDS)
    {
      chunk_len = BDOS_SHELL_IO_CHUNK_WORDS;
    }

    words_read = brfs_read(src_fd, chunk, (unsigned int)chunk_len);
    if (words_read <= 0)
    {
      if (words_read < 0)
      {
        bdos_shell_print_fs_error("mv: read", words_read);
      }
      break;
    }

    words_written = brfs_write(dst_fd, chunk, (unsigned int)words_read);
    if (words_written < 0)
    {
      bdos_shell_print_fs_error("mv: write", words_written);
      break;
    }

    remaining -= words_read;
  }

  brfs_close(src_fd);
  brfs_close(dst_fd);

  // Delete source only if copy succeeded (remaining == 0)
  if (remaining <= 0)
  {
    result = brfs_delete(src_resolved);
    if (result != BRFS_OK)
    {
      bdos_shell_print_fs_error("mv: delete source", result);
    }
  }

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

// Resolve a program name to a BRFS path.
// If name contains '/' or '.', resolves as a path (relative to cwd or absolute).
// Otherwise, tries /bin/<name>.
// Returns BRFS_OK on success with resolved path in out_path, or error code.
int bdos_shell_resolve_program(char* name, char* out_path)
{
  char bin_path[BDOS_SHELL_PATH_MAX];
  int result;

  // If it looks like a path (contains / or .), resolve directly
  if (strchr(name, '/') != 0 || name[0] == '.')
  {
    return bdos_shell_resolve_path(name, out_path);
  }

  // Bare name: try /bin/<name>
  strcpy(bin_path, "/bin/");
  strcat(bin_path, name);
  result = bdos_shell_resolve_path(bin_path, out_path);
  if (result == BRFS_OK && brfs_exists(out_path))
  {
    return BRFS_OK;
  }

  // Fallback: try resolving relative to cwd
  result = bdos_shell_resolve_path(name, out_path);
  return result;
}

int bdos_shell_cmd_jobs(int argc, char** argv)
{
  int i;
  int any;

  (void)argv;

  if (argc != 1)
  {
    term_puts("usage: jobs\n");
    return 0;
  }

  any = 0;
  for (i = 0; i < MEM_SLOT_COUNT; i++)
  {
    if (bdos_slot_status[i] != BDOS_SLOT_STATUS_EMPTY)
    {
      term_puts("[");
      term_putint(i);
      term_puts("] ");

      if (bdos_slot_status[i] == BDOS_SLOT_STATUS_RUNNING)
      {
        term_puts("running   ");
      }
      else if (bdos_slot_status[i] == BDOS_SLOT_STATUS_SUSPENDED)
      {
        term_puts("suspended ");
      }

      if (bdos_slot_name[i][0])
      {
        term_puts(bdos_slot_name[i]);
      }
      else
      {
        term_puts("(unnamed)");
      }
      term_putchar('\n');
      any = 1;
    }
  }

  if (!any)
  {
    term_puts("no active programs\n");
  }

  return 0;
}

int bdos_shell_cmd_kill(int argc, char** argv)
{
  int slot;

  if (argc != 2)
  {
    term_puts("usage: kill <slot>\n");
    return 0;
  }

  slot = atoi(argv[1]);
  if (slot < 0 || slot >= MEM_SLOT_COUNT)
  {
    term_puts("error: invalid slot number (0-");
    term_putint(MEM_SLOT_COUNT - 1);
    term_puts(")\n");
    return 0;
  }

  if (bdos_slot_status[slot] == BDOS_SLOT_STATUS_EMPTY)
  {
    term_puts("error: slot ");
    term_putint(slot);
    term_puts(" is empty\n");
    return 0;
  }

  if (bdos_slot_status[slot] == BDOS_SLOT_STATUS_RUNNING)
  {
    term_puts("error: cannot kill running program from shell\n");
    return 0;
  }

  term_puts("Killed [");
  term_putint(slot);
  term_puts("] ");
  term_puts(bdos_slot_name[slot]);
  term_putchar('\n');

  bdos_slot_free(slot);
  return 0;
}

int bdos_shell_cmd_fg(int argc, char** argv)
{
  int slot;

  if (argc != 2)
  {
    term_puts("usage: fg <slot>\n");
    return 0;
  }

  slot = atoi(argv[1]);
  if (slot < 0 || slot >= MEM_SLOT_COUNT)
  {
    term_puts("error: invalid slot number (0-");
    term_putint(MEM_SLOT_COUNT - 1);
    term_puts(")\n");
    return 0;
  }

  if (bdos_slot_status[slot] != BDOS_SLOT_STATUS_SUSPENDED)
  {
    term_puts("error: slot ");
    term_putint(slot);
    term_puts(" is not suspended\n");
    return 0;
  }

  term_puts("[");
  term_putint(slot);
  term_puts("] resuming: ");
  term_puts(bdos_slot_name[slot]);
  term_putchar('\n');

  bdos_resume_program(slot);
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
  char prog_path[BDOS_SHELL_PATH_MAX];
  int resolve_result;

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

  if (strcmp(argv[0], "cp") == 0)
  {
    bdos_shell_cmd_cp(argc, argv);
    return;
  }

  if (strcmp(argv[0], "mv") == 0)
  {
    bdos_shell_cmd_mv(argc, argv);
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

  if (strcmp(argv[0], "jobs") == 0)
  {
    bdos_shell_cmd_jobs(argc, argv);
    return;
  }

  if (strcmp(argv[0], "kill") == 0)
  {
    bdos_shell_cmd_kill(argc, argv);
    return;
  }

  if (strcmp(argv[0], "fg") == 0)
  {
    bdos_shell_cmd_fg(argc, argv);
    return;
  }

  // Not a built-in command: try to find and execute a program
  if (bdos_fs_ready)
  {
    resolve_result = bdos_shell_resolve_program(argv[0], prog_path);
    if (resolve_result == BRFS_OK && brfs_exists(prog_path) && !brfs_is_dir(prog_path))
    {
      // Store argc/argv for user program access via syscall
      bdos_shell_prog_argc = argc;
      {
        int i;
        for (i = 0; i < argc && i < BDOS_SHELL_ARGV_MAX; i++)
        {
          bdos_shell_prog_argv[i] = argv[i];
        }
      }
      bdos_exec_program(prog_path);
      bdos_shell_prog_argc = 0;
      return;
    }
  }

  term_puts(argv[0]);
  term_puts(": command not found\n");
}
