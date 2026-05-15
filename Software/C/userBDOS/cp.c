/*
 * cp — copy files and directories
 *
 * Usage: cp [-r] <source> <dest>
 * -r: copy directories recursively
 */

#include <syscall.h>
#include <string.h>

#define BUF_SIZE        256
#define MAX_DIR_ENTRIES 64
#define DIR_ENTRY_WORDS 8
#define FILENAME_WORDS  4
#define FLAG_DIRECTORY  0x01
#define MAX_PATH        256

static void decompress_name(char *dest, unsigned int *src)
{
    int wi;
    unsigned int word;
    unsigned int c;
    int ci;

    ci = 0;
    for (wi = 0; wi < FILENAME_WORDS; wi++)
    {
        word = src[wi];
        c = (word >> 24) & 0xFF; dest[ci++] = c; if (c == 0) return;
        c = (word >> 16) & 0xFF; dest[ci++] = c; if (c == 0) return;
        c = (word >> 8) & 0xFF;  dest[ci++] = c; if (c == 0) return;
        c = word & 0xFF;         dest[ci++] = c; if (c == 0) return;
    }
    dest[ci] = 0;
}

static int copy_file(const char *src, const char *dst)
{
    int src_fd;
    int dst_fd;
    char buf[BUF_SIZE];
    int n;

    src_fd = sys_open(src, O_RDONLY);
    if (src_fd < 0)
    {
        sys_putstr("cp: cannot open '");
        sys_putstr(src);
        sys_putstr("'\n");
        return 1;
    }

    dst_fd = sys_open(dst, O_WRONLY | O_CREAT | O_TRUNC);
    if (dst_fd < 0)
    {
        sys_putstr("cp: cannot create '");
        sys_putstr(dst);
        sys_putstr("'\n");
        sys_close(src_fd);
        return 1;
    }

    while ((n = sys_read(src_fd, buf, BUF_SIZE)) > 0)
    {
        sys_write(dst_fd, buf, n);
    }

    sys_close(src_fd);
    sys_close(dst_fd);
    return 0;
}

static int copy_recursive(const char *src, const char *dst)
{
    unsigned int entry_buf[MAX_DIR_ENTRIES * DIR_ENTRY_WORDS];
    int count;
    int i;
    char child_src[MAX_PATH];
    char child_dst[MAX_PATH];
    char name[20];
    unsigned int *entry;
    int ret;

    count = sys_readdir(src, entry_buf, MAX_DIR_ENTRIES);
    if (count < 0)
    {
        /* Not a directory — copy as file */
        return copy_file(src, dst);
    }

    /* Create destination directory */
    sys_mkdir(dst);

    ret = 0;
    for (i = 0; i < count; i++)
    {
        entry = entry_buf + (i * DIR_ENTRY_WORDS);
        decompress_name(name, entry);

        if (name[0] == '.' && (name[1] == '\0' ||
            (name[1] == '.' && name[2] == '\0')))
            continue;

        strcpy(child_src, src);
        if (child_src[strlen(child_src) - 1] != '/')
            strcat(child_src, "/");
        strcat(child_src, name);

        strcpy(child_dst, dst);
        if (child_dst[strlen(child_dst) - 1] != '/')
            strcat(child_dst, "/");
        strcat(child_dst, name);

        if (entry[5] & FLAG_DIRECTORY)
        {
            if (copy_recursive(child_src, child_dst) != 0)
                ret = 1;
        }
        else
        {
            if (copy_file(child_src, child_dst) != 0)
                ret = 1;
        }
    }
    return ret;
}

int main(void)
{
    int argc;
    char **argv;
    int rflag;
    int src_idx;
    int dst_idx;
    int i;

    argc = sys_argc();
    argv = sys_argv();

    rflag = 0;
    for (i = 1; i < argc; i++)
    {
        if (argv[i][0] == '-' && argv[i][1] == 'r' && argv[i][2] == '\0')
            rflag = 1;
    }

    /* Find src and dst (skip flags) */
    src_idx = -1;
    dst_idx = -1;
    for (i = 1; i < argc; i++)
    {
        if (argv[i][0] == '-') continue;
        if (src_idx < 0)
            src_idx = i;
        else
            dst_idx = i;
    }

    if (src_idx < 0 || dst_idx < 0)
    {
        sys_putstr("usage: cp [-r] <source> <dest>\n");
        return 1;
    }

    if (rflag)
        return copy_recursive(argv[src_idx], argv[dst_idx]);
    else
        return copy_file(argv[src_idx], argv[dst_idx]);
}
