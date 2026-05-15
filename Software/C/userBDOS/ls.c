/*
 * ls — list directory contents
 *
 * Usage: ls [path]
 * Defaults to current working directory if no path given.
 */

#include <syscall.h>
#include <string.h>
#include <stdlib.h>

#define MAX_ENTRIES     64
#define DIR_ENTRY_WORDS 8
#define FILENAME_WORDS  4
#define MAX_NAME_LEN    17
#define MAX_PATH_LEN    256

#define ENTRY_FILENAME  0
#define ENTRY_FLAGS     5
#define ENTRY_FILESIZE  7

#define FLAG_DIRECTORY  0x01

void decompress_name(char *dest, unsigned int *src)
{
    int wi;
    unsigned int word;
    unsigned int c;
    int ci;

    ci = 0;
    for (wi = 0; wi < FILENAME_WORDS; wi++)
    {
        word = src[wi];

        c = (word >> 24) & 0xFF;
        dest[ci++] = c;
        if (c == 0) return;

        c = (word >> 16) & 0xFF;
        dest[ci++] = c;
        if (c == 0) return;

        c = (word >> 8) & 0xFF;
        dest[ci++] = c;
        if (c == 0) return;

        c = word & 0xFF;
        dest[ci++] = c;
        if (c == 0) return;
    }
    dest[ci] = 0;
}

int main(void)
{
    int argc;
    char **argv;
    char cwd[128];
    char path[MAX_PATH_LEN];
    unsigned int *entry_buf;
    int count;
    int i;
    unsigned int *entry;
    char name[MAX_NAME_LEN];

    argc = sys_argc();
    argv = sys_argv();
    sys_getcwd(cwd, 128);

    if (argc > 2)
    {
        sys_putstr("usage: ls [path]\n");
        return 1;
    }

    if (argc == 2)
    {
        if (argv[1][0] == '/')
        {
            strcpy(path, argv[1]);
        }
        else
        {
            strcpy(path, cwd);
            if (strlen(cwd) > 1)
            {
                strcat(path, "/");
            }
            strcat(path, argv[1]);
        }
    }
    else
    {
        strcpy(path, cwd);
    }

    entry_buf = malloc(MAX_ENTRIES * DIR_ENTRY_WORDS * sizeof(unsigned int));
    if (entry_buf == 0)
    {
        sys_putstr("ls: allocation failed\n");
        return 1;
    }

    count = sys_readdir(path, entry_buf, MAX_ENTRIES);
    if (count < 0)
    {
        sys_putstr("ls: cannot access '");
        sys_putstr(path);
        sys_putstr("'\n");
        return 1;
    }

    for (i = 0; i < count; i++)
    {
        entry = entry_buf + (i * DIR_ENTRY_WORDS);
        decompress_name(name, entry + ENTRY_FILENAME);

        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
        {
            continue;
        }

        sys_putstr(name);
        if (entry[ENTRY_FLAGS] & FLAG_DIRECTORY)
        {
            sys_putc('/');
        }
        sys_putc('\n');
    }

    return 0;
}
