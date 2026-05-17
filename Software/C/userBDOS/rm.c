/*
 * rm — remove files and directories
 *
 * Usage: rm [-r] <file> [file ...]
 * -r: remove directories recursively
 */

#include <syscall.h>
#include <string.h>

#define MAX_DIR_ENTRIES 64
#define DIR_ENTRY_WORDS 8
#define FILENAME_WORDS  4
#define FLAG_DIRECTORY  0x01
#define MAX_PATH        256

static int rflag;

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

static int rm_recursive(const char *path)
{
    unsigned int entry_buf[MAX_DIR_ENTRIES * DIR_ENTRY_WORDS];
    int count;
    int i;
    char child[MAX_PATH];
    char name[20];
    unsigned int *entry;
    int ret;

    count = sys_readdir(path, entry_buf, MAX_DIR_ENTRIES);
    if (count < 0)
    {
        /* Not a directory — try to unlink as file */
        return sys_unlink(path);
    }

    /* It's a directory — remove children first */
    ret = 0;
    for (i = 0; i < count; i++)
    {
        entry = entry_buf + (i * DIR_ENTRY_WORDS);
        decompress_name(name, entry);

        if (name[0] == '.' && (name[1] == '\0' ||
            (name[1] == '.' && name[2] == '\0')))
            continue;

        strcpy(child, path);
        if (child[strlen(child) - 1] != '/')
            strcat(child, "/");
        strcat(child, name);

        if (entry[5] & FLAG_DIRECTORY)
        {
            if (rm_recursive(child) < 0)
                ret = -1;
        }
        else
        {
            if (sys_unlink(child) < 0)
            {
                sys_putstr("rm: cannot remove '");
                sys_putstr(child);
                sys_putstr("'\n");
                ret = -1;
            }
        }
    }

    /* Remove the now-empty directory itself */
    if (sys_unlink(path) < 0)
    {
        sys_putstr("rm: cannot remove '");
        sys_putstr(path);
        sys_putstr("'\n");
        ret = -1;
    }
    return ret;
}

int main(void)
{
    int argc;
    char **argv;
    int i;
    int ret;

    argc = sys_argc();
    argv = sys_argv();

    rflag = 0;
    for (i = 1; i < argc; i++)
    {
        if (argv[i][0] == '-' && argv[i][1] == 'r' && argv[i][2] == '\0')
            rflag = 1;
    }

    if (argc < 2 || (rflag && argc < 3))
    {
        sys_putstr("usage: rm [-r] <file> [file ...]\n");
        return 1;
    }

    ret = 0;
    for (i = 1; i < argc; i++)
    {
        if (argv[i][0] == '-') continue;
        if (rflag)
        {
            if (rm_recursive(argv[i]) < 0)
                ret = 1;
        }
        else
        {
            if (sys_unlink(argv[i]) < 0)
            {
                sys_putstr("rm: cannot remove '");
                sys_putstr(argv[i]);
                sys_putstr("'\n");
                ret = 1;
            }
        }
    }
    return ret;
}
