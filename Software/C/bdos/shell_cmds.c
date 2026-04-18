#include "bdos.h"

#define BDOS_SHELL_LS_MAX_ENTRIES 32
#define BDOS_SHELL_IO_CHUNK_WORDS 64

/* (bdos_shell_prog_argc / argv now live in shell_exec.c.) */

/* ---- Built-in commands ---- */

static int bdos_shell_cmd_help(int argc, char **argv)
{
  (void)argc;
  (void)argv;

  term2_puts("BDOS shell help\n");
  term2_puts("--------------\n");
  term2_puts("General\n");
  term2_puts("  help  clear  echo  uptime\n");
  term2_puts("Filesystem\n");
  term2_puts("  pwd  cd  ls  df\n");
  term2_puts("  mkdir  mkfile  rm\n");
  term2_puts("  cat  write  cp  mv\n");
  term2_puts("Maintenance\n");
  term2_puts("  sync   (use /bin/format to format)\n");
  term2_puts("Programs\n");
  term2_puts("  path, or filename from /bin\n");
  term2_puts("  jobs  fg <slot>  kill <slot>\n");
  term2_puts("Hotkeys (during program)\n");
  term2_puts("  F1=shell  Alt+F4=kill\n");
  return 0;
}

static int bdos_shell_cmd_clear(int argc, char **argv)
{
  (void)argc;
  (void)argv;
  term2_clear();
  return 0;
}

static int bdos_shell_cmd_echo(int argc, char **argv)
{
  int i;

  for (i = 1; i < argc; i++)
  {
    term2_puts(argv[i]);
    if (i < argc - 1)
    {
      term2_putchar(' ');
    }
  }
  term2_putchar('\n');

  return 0;
}

static int bdos_shell_cmd_uptime(int argc, char **argv)
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

  term2_puts("Uptime: ");
  term2_putint((int)days);
  term2_puts("d ");
  bdos_shell_print_2digit(hours);
  term2_puts("h ");
  bdos_shell_print_2digit(minutes);
  term2_puts("m ");
  bdos_shell_print_2digit(seconds);
  term2_puts("s\n");
  return 0;
}

static int bdos_shell_cmd_pwd(int argc, char **argv)
{
  (void)argc;
  (void)argv;
  term2_puts(bdos_shell_cwd);
  term2_putchar('\n');
  return 0;
}

static int bdos_shell_cmd_cd(int argc, char **argv)
{
  char resolved[BDOS_SHELL_PATH_MAX];
  int result;

  if (!bdos_shell_require_fs_ready())
  {
    return 0;
  }

  if (argc == 1)
  {
    strcpy(bdos_shell_cwd, "/");
    return 0;
  }

  if (argc != 2)
  {
    term2_puts("usage: cd [path]\n");
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
    term2_puts("error: not a directory\n");
    return 0;
  }

  strcpy(bdos_shell_cwd, resolved);
  return 0;
}

static int bdos_shell_cmd_ls(int argc, char **argv)
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
    term2_puts("usage: ls [path]\n");
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
    term2_puts(dir_names[i]);
    term2_putchar('\n');
  }

  for (i = 0; i < file_count; i++)
  {
    term2_puts(file_names[i]);

    size_len = bdos_shell_format_byte_size(file_sizes[i], size_buf);
    prefix_len = 2 + strlen(file_names[i]);
    spaces = size_col - prefix_len;
    if (spaces < 1)
    {
      spaces = 1;
    }

    while (spaces > 0)
    {
      term2_putchar(' ');
      spaces--;
    }

    term2_puts(size_buf);
    term2_putchar('\n');
  }

  return 0;
}

static int bdos_shell_cmd_mkdir(int argc, char **argv)
{
  char resolved[BDOS_SHELL_PATH_MAX];
  int result;

  if (!bdos_shell_require_fs_ready())
  {
    return 0;
  }

  if (argc != 2)
  {
    term2_puts("usage: mkdir <path>\n");
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

static int bdos_shell_cmd_mkfile(int argc, char **argv)
{
  char resolved[BDOS_SHELL_PATH_MAX];
  int result;

  if (!bdos_shell_require_fs_ready())
  {
    return 0;
  }

  if (argc != 2)
  {
    term2_puts("usage: mkfile <path>\n");
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

static int bdos_shell_rm_recursive(char *path)
{
  struct brfs_dir_entry entries[BDOS_SHELL_LS_MAX_ENTRIES];
  char name[BRFS_MAX_FILENAME_LENGTH + 1];
  char child_path[BDOS_SHELL_PATH_MAX];
  int count;
  int i;
  int result;
  int path_len;

  count = brfs_read_dir(path, entries, BDOS_SHELL_LS_MAX_ENTRIES);
  if (count < 0)
  {
    return count;
  }

  path_len = strlen(path);

  for (i = 0; i < count; i++)
  {
    brfs_decompress_string(name, entries[i].filename, 4);

    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
    {
      continue;
    }

    strcpy(child_path, path);
    if (path_len > 1)
    {
      strcat(child_path, "/");
    }
    strcat(child_path, name);

    if (entries[i].flags & BRFS_FLAG_DIRECTORY)
    {
      result = bdos_shell_rm_recursive(child_path);
      if (result != BRFS_OK)
      {
        return result;
      }
    }

    result = brfs_delete(child_path);
    if (result != BRFS_OK)
    {
      return result;
    }
  }

  return BRFS_OK;
}

static int bdos_shell_cmd_rm(int argc, char **argv)
{
  char resolved[BDOS_SHELL_PATH_MAX];
  int result;
  int recursive;
  char *path_arg;

  if (!bdos_shell_require_fs_ready())
  {
    return 0;
  }

  recursive = 0;
  path_arg = NULL;

  if (argc == 2)
  {
    if (strcmp(argv[1], "-r") == 0)
    {
      term2_puts("usage: rm [-r] <path>\n");
      return 0;
    }
    path_arg = argv[1];
  }
  else if (argc == 3)
  {
    if (strcmp(argv[1], "-r") == 0)
    {
      recursive = 1;
      path_arg = argv[2];
    }
    else
    {
      term2_puts("usage: rm [-r] <path>\n");
      return 0;
    }
  }
  else
  {
    term2_puts("usage: rm [-r] <path>\n");
    return 0;
  }

  result = bdos_shell_resolve_path(path_arg, resolved);
  if (result != BRFS_OK)
  {
    bdos_shell_print_fs_error("resolve path", result);
    return 0;
  }

  if (recursive)
  {
    result = bdos_shell_rm_recursive(resolved);
    if (result != BRFS_OK)
    {
      bdos_shell_print_fs_error("rm", result);
      return 0;
    }
  }

  result = brfs_delete(resolved);
  if (result != BRFS_OK)
  {
    bdos_shell_print_fs_error("rm", result);
  }

  return 0;
}

static int bdos_shell_cmd_cat(int argc, char **argv)
{
  char resolved[BDOS_SHELL_PATH_MAX];
  char buf[256];
  int fd;
  int n;
  int result;

  if (!bdos_shell_require_fs_ready())
  {
    return 0;
  }

  if (argc != 2)
  {
    term2_puts("usage: cat <path>\n");
    return 0;
  }

  result = bdos_shell_resolve_path(argv[1], resolved);
  if (result != BRFS_OK)
  {
    bdos_shell_print_fs_error("resolve path", result);
    return 0;
  }

  fd = bdos_vfs_open(resolved, BDOS_O_RDONLY);
  if (fd < 0)
  {
    bdos_shell_print_fs_error("open", fd);
    return 0;
  }

  for (;;)
  {
    n = bdos_vfs_read(fd, buf, sizeof(buf));
    if (n <= 0) break;
    bdos_vfs_write(1, buf, n);
  }

  bdos_vfs_close(fd);
  return 0;
}

static int bdos_shell_cmd_write(int argc, char **argv)
{
  char resolved[BDOS_SHELL_PATH_MAX];
  unsigned char buf[BDOS_SHELL_INPUT_MAX];
  int result;
  int fd;
  int i;
  int j;
  int byte_count;

  if (!bdos_shell_require_fs_ready())
  {
    return 0;
  }

  if (argc < 3)
  {
    term2_puts("usage: write <path> <text>\n");
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
      term2_puts("error: cannot write to directory\n");
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

  byte_count = 0;
  for (i = 2; i < argc; i++)
  {
    if (i > 2)
    {
      if (byte_count >= BDOS_SHELL_INPUT_MAX)
      {
        term2_puts("error: text too long\n");
        brfs_close(fd);
        return 0;
      }
      buf[byte_count++] = ' ';
    }

    for (j = 0; argv[i][j] != '\0'; j++)
    {
      if (byte_count >= BDOS_SHELL_INPUT_MAX)
      {
        term2_puts("error: text too long\n");
        brfs_close(fd);
        return 0;
      }
      buf[byte_count++] = (unsigned char)argv[i][j];
    }
  }

  result = brfs_write(fd, buf, (unsigned int)byte_count);
  if (result < 0)
  {
    bdos_shell_print_fs_error("write", result);
    brfs_close(fd);
    return 0;
  }

  brfs_close(fd);

  term2_puts("wrote ");
  term2_putint(byte_count);
  term2_puts(" bytes\n");
  return 0;
}

static int bdos_shell_cmd_cp(int argc, char **argv)
{
  char src_resolved[BDOS_SHELL_PATH_MAX];
  char dst_resolved[BDOS_SHELL_PATH_MAX];
  unsigned char chunk[BDOS_SHELL_IO_CHUNK_WORDS * 4];
  int result;
  int src_fd;
  int dst_fd;
  int remaining;
  int chunk_len;
  int bytes_read;
  int bytes_written;
  char *slash;

  if (!bdos_shell_require_fs_ready())
  {
    return 0;
  }

  if (argc != 3)
  {
    term2_puts("usage: cp <source> <dest>\n");
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
    term2_puts("cp: source not found\n");
    return 0;
  }

  if (brfs_is_dir(src_resolved))
  {
    term2_puts("cp: cannot copy a directory\n");
    return 0;
  }

  result = bdos_shell_resolve_path(argv[2], dst_resolved);
  if (result != BRFS_OK)
  {
    bdos_shell_print_fs_error("resolve dest", result);
    return 0;
  }

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

  if (brfs_exists(dst_resolved) && !brfs_is_dir(dst_resolved))
  {
    result = brfs_delete(dst_resolved);
    if (result != BRFS_OK)
    {
      bdos_shell_print_fs_error("cp: replace dest", result);
      return 0;
    }
  }

  result = brfs_create_file(dst_resolved);
  if (result != BRFS_OK)
  {
    bdos_shell_print_fs_error("cp: create dest", result);
    return 0;
  }

  src_fd = brfs_open(src_resolved);
  if (src_fd < 0)
  {
    bdos_shell_print_fs_error("cp: open source", src_fd);
    return 0;
  }

  dst_fd = brfs_open(dst_resolved);
  if (dst_fd < 0)
  {
    bdos_shell_print_fs_error("cp: open dest", dst_fd);
    brfs_close(src_fd);
    return 0;
  }

  remaining = brfs_file_size(src_fd);
  while (remaining > 0)
  {
    chunk_len = remaining;
    if (chunk_len > (int)sizeof(chunk))
    {
      chunk_len = (int)sizeof(chunk);
    }

    bytes_read = brfs_read(src_fd, chunk, (unsigned int)chunk_len);
    if (bytes_read <= 0)
    {
      if (bytes_read < 0)
      {
        bdos_shell_print_fs_error("cp: read", bytes_read);
      }
      break;
    }

    bytes_written = brfs_write(dst_fd, chunk, (unsigned int)bytes_read);
    if (bytes_written < 0)
    {
      bdos_shell_print_fs_error("cp: write", bytes_written);
      break;
    }

    remaining -= bytes_read;
  }

  brfs_close(src_fd);
  brfs_close(dst_fd);
  return 0;
}

static int bdos_shell_cmd_mv(int argc, char **argv)
{
  char src_resolved[BDOS_SHELL_PATH_MAX];
  char dst_resolved[BDOS_SHELL_PATH_MAX];
  unsigned char chunk[BDOS_SHELL_IO_CHUNK_WORDS * 4];
  int result;
  int src_fd;
  int dst_fd;
  int remaining;
  int chunk_len;
  int bytes_read;
  int bytes_written;
  char *slash;

  if (!bdos_shell_require_fs_ready())
  {
    return 0;
  }

  if (argc != 3)
  {
    term2_puts("usage: mv <source> <dest>\n");
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
    term2_puts("mv: source not found\n");
    return 0;
  }

  if (brfs_is_dir(src_resolved))
  {
    term2_puts("mv: cannot move a directory\n");
    return 0;
  }

  result = bdos_shell_resolve_path(argv[2], dst_resolved);
  if (result != BRFS_OK)
  {
    bdos_shell_print_fs_error("resolve dest", result);
    return 0;
  }

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

  if (brfs_exists(dst_resolved) && !brfs_is_dir(dst_resolved))
  {
    result = brfs_delete(dst_resolved);
    if (result != BRFS_OK)
    {
      bdos_shell_print_fs_error("mv: replace dest", result);
      return 0;
    }
  }

  result = brfs_create_file(dst_resolved);
  if (result != BRFS_OK)
  {
    bdos_shell_print_fs_error("mv: create dest", result);
    return 0;
  }

  src_fd = brfs_open(src_resolved);
  if (src_fd < 0)
  {
    bdos_shell_print_fs_error("mv: open source", src_fd);
    return 0;
  }

  dst_fd = brfs_open(dst_resolved);
  if (dst_fd < 0)
  {
    bdos_shell_print_fs_error("mv: open dest", dst_fd);
    brfs_close(src_fd);
    return 0;
  }

  remaining = brfs_file_size(src_fd);
  while (remaining > 0)
  {
    chunk_len = remaining;
    if (chunk_len > (int)sizeof(chunk))
    {
      chunk_len = (int)sizeof(chunk);
    }

    bytes_read = brfs_read(src_fd, chunk, (unsigned int)chunk_len);
    if (bytes_read <= 0)
    {
      if (bytes_read < 0)
      {
        bdos_shell_print_fs_error("mv: read", bytes_read);
      }
      break;
    }

    bytes_written = brfs_write(dst_fd, chunk, (unsigned int)bytes_read);
    if (bytes_written < 0)
    {
      bdos_shell_print_fs_error("mv: write", bytes_written);
      break;
    }

    remaining -= bytes_read;
  }

  brfs_close(src_fd);
  brfs_close(dst_fd);

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

static int bdos_shell_cmd_sync(int argc, char **argv)
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

static int bdos_shell_cmd_df(int argc, char **argv)
{
  unsigned int total_blocks;
  unsigned int free_blocks;
  unsigned int words_per_block;
  unsigned int used_blocks;
  unsigned int total_bytes;
  unsigned int used_bytes;
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
  total_bytes = total_blocks * words_per_block * 4u;
  used_bytes  = used_blocks  * words_per_block * 4u;
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

  term2_puts(line_header);
  term2_putchar('\n');
  bdos_shell_print_hline((unsigned int)line_len);

  bdos_shell_print_field_prefix("Total:", value_col);
  bdos_shell_print_kib(total_bytes);
  term2_putchar('\n');

  bdos_shell_print_field_prefix("Used:", value_col);
  bdos_shell_print_kib(used_bytes);
  term2_puts(" (");
  term2_putint((int)usage_percent);
  term2_puts("%)\n");

  bdos_shell_print_field_prefix("Blocks:", value_col);
  bdos_shell_u32_to_str(used_blocks, value_buf);
  term2_puts(value_buf);
  term2_putchar('/');
  bdos_shell_u32_to_str(total_blocks, value_buf);
  term2_puts(value_buf);
  term2_puts(" used\n");

  bdos_shell_print_field_prefix("Block size:", value_col);
  term2_putint((int)(words_per_block * 4));
  term2_puts(" B\n");

  return 0;
}

int bdos_shell_resolve_program(char *name, char *out_path)
{
  char         search[BDOS_SHELL_PATH_MAX];
  const char  *path;
  const char  *p;
  int          result;

  /* Explicit path (contains '/' or starts with '.') skips $PATH. */
  if (strchr(name, '/') != 0 || name[0] == '.')
  {
    return bdos_shell_resolve_path(name, out_path);
  }

  path = bdos_shell_var_get("PATH");
  if (path == 0 || path[0] == 0) path = "/bin";

  /* Walk PATH, splitting on ':'. For each entry build "<entry>/<name>"
   * and try to resolve. First hit wins. */
  p = path;
  while (*p)
  {
    int n = 0;
    while (*p && *p != ':' && n < BDOS_SHELL_PATH_MAX - 2)
      search[n++] = *p++;
    if (n == 0) { if (*p == ':') p++; continue; }
    if (search[n - 1] != '/') search[n++] = '/';
    search[n] = 0;
    if ((int)strlen(name) + n >= BDOS_SHELL_PATH_MAX) {
      if (*p == ':') p++;
      continue;
    }
    strcat(search, name);
    result = bdos_shell_resolve_path(search, out_path);
    if (result == BRFS_OK && brfs_exists(out_path) && !brfs_is_dir(out_path))
      return BRFS_OK;
    if (*p == ':') p++;
  }

  /* Fallback: resolve as-is against cwd. */
  return bdos_shell_resolve_path(name, out_path);
}

static int bdos_shell_cmd_jobs(int argc, char **argv)
{
  int i;
  int any;

  (void)argv;

  if (argc != 1)
  {
    term2_puts("usage: jobs\n");
    return 0;
  }

  any = 0;
  for (i = 0; i < MEM_SLOT_COUNT; i++)
  {
    if (bdos_slot_status[i] != BDOS_SLOT_STATUS_EMPTY)
    {
      term2_puts("[");
      term2_putint(i);
      term2_puts("] ");

      if (bdos_slot_status[i] == BDOS_SLOT_STATUS_RUNNING)
      {
        term2_puts("running   ");
      }
      else if (bdos_slot_status[i] == BDOS_SLOT_STATUS_SUSPENDED)
      {
        term2_puts("suspended ");
      }

      if (bdos_slot_name[i][0])
      {
        term2_puts(bdos_slot_name[i]);
      }
      else
      {
        term2_puts("(unnamed)");
      }
      term2_putchar('\n');
      any = 1;
    }
  }

  if (!any)
  {
    term2_puts("no active programs\n");
  }

  return 0;
}

static int bdos_shell_cmd_kill(int argc, char **argv)
{
  int slot;

  if (argc != 2)
  {
    term2_puts("usage: kill <slot>\n");
    return 0;
  }

  slot = atoi(argv[1]);
  if (slot < 0 || slot >= MEM_SLOT_COUNT)
  {
    term2_puts("error: invalid slot number (0-");
    term2_putint(MEM_SLOT_COUNT - 1);
    term2_puts(")\n");
    return 0;
  }

  if (bdos_slot_status[slot] == BDOS_SLOT_STATUS_EMPTY)
  {
    term2_puts("error: slot ");
    term2_putint(slot);
    term2_puts(" is empty\n");
    return 0;
  }

  if (bdos_slot_status[slot] == BDOS_SLOT_STATUS_RUNNING)
  {
    term2_puts("error: cannot kill running program from shell\n");
    return 0;
  }

  term2_puts("Killed [");
  term2_putint(slot);
  term2_puts("] ");
  term2_puts(bdos_slot_name[slot]);
  term2_putchar('\n');

  bdos_slot_free(slot);
  return 0;
}

static int bdos_shell_cmd_fg(int argc, char **argv)
{
  int slot;

  if (argc != 2)
  {
    term2_puts("usage: fg <slot>\n");
    return 0;
  }

  slot = atoi(argv[1]);
  if (slot < 0 || slot >= MEM_SLOT_COUNT)
  {
    term2_puts("error: invalid slot number (0-");
    term2_putint(MEM_SLOT_COUNT - 1);
    term2_puts(")\n");
    return 0;
  }

  if (bdos_slot_status[slot] != BDOS_SLOT_STATUS_SUSPENDED)
  {
    term2_puts("error: slot ");
    term2_putint(slot);
    term2_puts(" is not suspended\n");
    return 0;
  }

  term2_puts("[");
  term2_putint(slot);
  term2_puts("] resuming: ");
  term2_puts(bdos_slot_name[slot]);
  term2_putchar('\n');

  bdos_resume_program(slot);
  return 0;
}

/* `format` was a built-in here in BDOS v3 / shell-terminal-v2 phases A-D.
   Phase E moved it to an external userBDOS binary at /bin/format that
   wraps SYSCALL_FS_FORMAT. The kernel still owns the boot-time
   mount-failure path via shell_format.c so the system can be brought
   up on an erased flash before any user binary exists. */

/* ---- Command dispatcher ---- */

/* ---- New built-ins (variables / control) ---- */

static int bi_export_impl(int argc, char **argv)
{
  int i;
  if (argc < 2) {
    /* Print all exported vars. */
    bdos_shell_vars_print(1);
    return 0;
  }
  for (i = 1; i < argc; i++) {
    char *eq = strchr(argv[i], '=');
    if (eq) {
      char name[BDOS_SHELL_VAR_NAME];
      int  k;
      int  len = (int)(eq - argv[i]);
      if (len <= 0 || len >= BDOS_SHELL_VAR_NAME) continue;
      for (k = 0; k < len; k++) name[k] = argv[i][k];
      name[len] = 0;
      bdos_shell_var_set(name, eq + 1);
      bdos_shell_var_export(name);
    } else {
      bdos_shell_var_export(argv[i]);
    }
  }
  return 0;
}

static int bi_set_impl(int argc, char **argv)
{
  int i;
  if (argc < 2) {
    bdos_shell_vars_print(0);
    return 0;
  }
  for (i = 1; i < argc; i++) {
    char *eq = strchr(argv[i], '=');
    if (!eq) continue;
    {
      char name[BDOS_SHELL_VAR_NAME];
      int  k;
      int  len = (int)(eq - argv[i]);
      if (len <= 0 || len >= BDOS_SHELL_VAR_NAME) continue;
      for (k = 0; k < len; k++) name[k] = argv[i][k];
      name[len] = 0;
      bdos_shell_var_set(name, eq + 1);
    }
  }
  return 0;
}

static int bi_unset_impl(int argc, char **argv)
{
  int i;
  for (i = 1; i < argc; i++) bdos_shell_var_unset(argv[i]);
  return 0;
}

static int bi_env_impl(int argc, char **argv)
{
  (void)argc; (void)argv;
  bdos_shell_vars_print(1);
  return 0;
}

static int bi_exit_impl(int argc, char **argv)
{
  (void)argc; (void)argv;
  term2_puts("shell: exit not supported (BDOS shell is the system console)\n");
  return 1;
}

static int bi_true_impl (int argc, char **argv) { (void)argc; (void)argv; return 0; }
static int bi_false_impl(int argc, char **argv) { (void)argc; (void)argv; return 1; }

/* ---- bi_* wrappers exposed to shell_exec.c builtin registry ---- */

int bi_help   (int argc, char **argv) { return bdos_shell_cmd_help   (argc, argv); }
int bi_clear  (int argc, char **argv) { return bdos_shell_cmd_clear  (argc, argv); }
int bi_echo   (int argc, char **argv) { return bdos_shell_cmd_echo   (argc, argv); }
int bi_uptime (int argc, char **argv) { return bdos_shell_cmd_uptime (argc, argv); }
int bi_pwd    (int argc, char **argv) { return bdos_shell_cmd_pwd    (argc, argv); }
int bi_cd     (int argc, char **argv) { return bdos_shell_cmd_cd     (argc, argv); }
int bi_ls     (int argc, char **argv) { return bdos_shell_cmd_ls     (argc, argv); }
int bi_mkdir  (int argc, char **argv) { return bdos_shell_cmd_mkdir  (argc, argv); }
int bi_mkfile (int argc, char **argv) { return bdos_shell_cmd_mkfile (argc, argv); }
int bi_rm     (int argc, char **argv) { return bdos_shell_cmd_rm     (argc, argv); }
int bi_cat    (int argc, char **argv) { return bdos_shell_cmd_cat    (argc, argv); }
int bi_write  (int argc, char **argv) { return bdos_shell_cmd_write  (argc, argv); }
int bi_cp     (int argc, char **argv) { return bdos_shell_cmd_cp     (argc, argv); }
int bi_mv     (int argc, char **argv) { return bdos_shell_cmd_mv     (argc, argv); }
int bi_sync   (int argc, char **argv) { return bdos_shell_cmd_sync   (argc, argv); }
int bi_df     (int argc, char **argv) { return bdos_shell_cmd_df     (argc, argv); }
int bi_jobs   (int argc, char **argv) { return bdos_shell_cmd_jobs   (argc, argv); }
int bi_kill   (int argc, char **argv) { return bdos_shell_cmd_kill   (argc, argv); }
int bi_fg     (int argc, char **argv) { return bdos_shell_cmd_fg     (argc, argv); }

int bi_export (int argc, char **argv) { return bi_export_impl(argc, argv); }
int bi_set    (int argc, char **argv) { return bi_set_impl   (argc, argv); }
int bi_unset  (int argc, char **argv) { return bi_unset_impl (argc, argv); }
int bi_env    (int argc, char **argv) { return bi_env_impl   (argc, argv); }
int bi_exit   (int argc, char **argv) { return bi_exit_impl  (argc, argv); }
int bi_true   (int argc, char **argv) { return bi_true_impl  (argc, argv); }
int bi_false  (int argc, char **argv) { return bi_false_impl (argc, argv); }

