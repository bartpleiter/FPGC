/*
 * sdformat.c — userland front-end for SYSCALL_SD_FORMAT.
 *
 * Formats the SD card with a BRFS v2 filesystem. The SD card must be
 * detected and initialised at boot (bdos_fs_sd_init) for this to work.
 *
 * Usage:
 *   sdformat <blocks> <bytes-per-block> <label>
 *
 * Notes:
 *   - <blocks> must be a multiple of 64.
 *   - <bytes-per-block> must be a multiple of 4 (converted to words
 *     per block internally, matching brfs_format()).
 *   - <label> max 10 characters.
 *   - Always performs a full format.
 *
 * Example (4 MiB partition, 4096-byte blocks):
 *   sdformat 1024 4096 sdcard
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
  puts2("usage: sdformat <blocks (mult of 64)> <bytes-per-block (mult of 4)> <label>\n");
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
    puts2("sdformat: invalid block count\n");
    return 1;
  }
  if ((blocks & 63) != 0) {
    puts2("sdformat: block count must be a multiple of 64\n");
    return 1;
  }

  if (parse_uint(argv[2], &bytes_per_block) != 0 || bytes_per_block <= 0) {
    puts2("sdformat: invalid bytes-per-block\n");
    return 1;
  }
  if ((bytes_per_block & 3) != 0) {
    puts2("sdformat: bytes-per-block must be a multiple of 4\n");
    return 1;
  }
  words_per_block = bytes_per_block / 4;

  label = argv[3];
  if (slen(label) == 0 || slen(label) > LABEL_MAX) {
    puts2("sdformat: label must be 1..10 characters\n");
    return 1;
  }

  puts1("Formatting SD card (this may take a while)...\n");
  rc = sys_sd_format(blocks, words_per_block, label);
  if (rc != 0) {
    puts2("sdformat: format failed (is SD card inserted?)\n");
    return 1;
  }
  puts1("SD card format complete.\n");
  return 0;
}
