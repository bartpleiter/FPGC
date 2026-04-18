/*
 * format.c — userland front-end for SYSCALL_FS_FORMAT.
 *
 * Performs a BRFS v2 format + sync. Replaces the old in-shell `format`
 * builtin (removed in shell-terminal-v2 Phase E). The boot-time mount-
 * failure prompt that runs before any /bin program is available still
 * lives in the kernel (shell_format.c).
 *
 * Usage:
 *   format <blocks> <bytes-per-block> <label>
 *
 * Notes:
 *   - <blocks> must be a multiple of 64.
 *   - <bytes-per-block> must be a multiple of 4 (it is converted to
 *     "words per block" internally, matching brfs_format()).
 *   - <label> max 10 characters.
 *   - Always performs a *full* format (matches v3 wizard default).
 */

#include <syscall.h>

#define LABEL_MAX 10

/* ---- minimal helpers (no libc printf in userBDOS by default) ---- */

static int slen(const char *s) { int n = 0; while (s[n]) n++; return n; }
static void puts1(const char *s) { sys_write(1, s, slen(s)); }
static void puts2(const char *s) { sys_write(2, s, slen(s)); }

static int parse_uint(const char *s, int *out)
{
  int v = 0;
  int any = 0;
  if (!s || !*s) return -1;
  while (*s >= '0' && *s <= '9') {
    v = v * 10 + (*s - '0');
    s++;
    any = 1;
  }
  if (*s != '\0' || !any) return -1;
  *out = v;
  return 0;
}

static void usage(void)
{
  puts2("usage: format <blocks (mult of 64)> <bytes-per-block (mult of 4)> <label>\n");
}

int main(void)
{
  int    argc = sys_shell_argc();
  char **argv = sys_shell_argv();
  int    blocks;
  int    bytes_per_block;
  int    words_per_block;
  char  *label;
  int    rc;

  if (argc != 4) { usage(); return 1; }

  if (parse_uint(argv[1], &blocks) != 0 || blocks <= 0) {
    puts2("format: invalid block count\n");
    return 1;
  }
  if ((blocks & 63) != 0) {
    puts2("format: block count must be a multiple of 64\n");
    return 1;
  }

  if (parse_uint(argv[2], &bytes_per_block) != 0 || bytes_per_block <= 0) {
    puts2("format: invalid bytes-per-block\n");
    return 1;
  }
  if ((bytes_per_block & 3) != 0) {
    puts2("format: bytes-per-block must be a multiple of 4\n");
    return 1;
  }
  words_per_block = bytes_per_block / 4;

  label = argv[3];
  if (slen(label) == 0 || slen(label) > LABEL_MAX) {
    puts2("format: label must be 1..10 characters\n");
    return 1;
  }

  puts1("Formatting (this may take a while)...\n");
  rc = sys_fs_format(blocks, words_per_block, label);
  if (rc != 0) {
    puts2("format: SYSCALL_FS_FORMAT failed\n");
    return 1;
  }
  puts1("Format complete.\n");
  return 0;
}
