/*
 * ls — list directory contents
 *
 * Usage: ls [-l] [path]
 * Defaults to current working directory if no path given.
 * -l shows file sizes in long format.
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

struct ls_entry {
    char name[MAX_NAME_LEN];
    int  is_dir;
    unsigned int size;
};

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

static int my_strcmp(const char *a, const char *b)
{
    while (*a && *b && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

static void print_num(unsigned int val)
{
    char tmp[12];
    int i;
    int j;
    char out[12];

    if (val == 0)
    {
        sys_putc('0');
        return;
    }

    i = 0;
    while (val > 0)
    {
        tmp[i++] = '0' + (val % 10);
        val = val / 10;
    }

    j = 0;
    while (i > 0)
        out[j++] = tmp[--i];
    out[j] = 0;
    sys_putstr(out);
}

/* Right-justify a number in a field of given width */
static void print_num_rjust(unsigned int val, int width)
{
    char tmp[12];
    int i;
    int j;
    int pad;

    if (val == 0)
    {
        for (pad = 0; pad < width - 1; pad++)
            sys_putc(' ');
        sys_putc('0');
        return;
    }

    i = 0;
    while (val > 0)
    {
        tmp[i++] = '0' + (val % 10);
        val = val / 10;
    }

    for (pad = 0; pad < width - i; pad++)
        sys_putc(' ');
    for (j = i - 1; j >= 0; j--)
        sys_putc(tmp[j]);
}

int main(void)
{
    int argc;
    char **argv;
    char path[MAX_PATH_LEN];
    unsigned int *entry_buf;
    int count;
    int i;
    int j;
    unsigned int *entry;
    char name[MAX_NAME_LEN];
    int long_fmt;
    int path_arg;
    struct ls_entry entries_sorted[MAX_ENTRIES];
    int n_entries;

    argc = sys_argc();
    argv = sys_argv();

    /* Parse options */
    long_fmt = 0;
    path_arg = -1;
    for (i = 1; i < argc; i++)
    {
        if (argv[i][0] == '-' && argv[i][1] == 'l' && argv[i][2] == '\0')
        {
            long_fmt = 1;
        }
        else
        {
            path_arg = i;
        }
    }

    if (path_arg >= 0)
    {
        strcpy(path, argv[path_arg]);
    }
    else
    {
        sys_getcwd(path, MAX_PATH_LEN);
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

    /* Extract entries, skipping . and .. */
    n_entries = 0;
    for (i = 0; i < count && n_entries < MAX_ENTRIES; i++)
    {
        entry = entry_buf + (i * DIR_ENTRY_WORDS);
        decompress_name(name, entry + ENTRY_FILENAME);

        if (name[0] == '.' && (name[1] == '\0' ||
            (name[1] == '.' && name[2] == '\0')))
            continue;

        for (j = 0; name[j]; j++)
            entries_sorted[n_entries].name[j] = name[j];
        entries_sorted[n_entries].name[j] = '\0';
        entries_sorted[n_entries].is_dir = (entry[ENTRY_FLAGS] & FLAG_DIRECTORY) ? 1 : 0;
        entries_sorted[n_entries].size = entry[ENTRY_FILESIZE];
        n_entries++;
    }

    /* Bubble sort alphabetically */
    for (i = 0; i < n_entries - 1; i++)
    {
        for (j = 0; j < n_entries - 1 - i; j++)
        {
            if (my_strcmp(entries_sorted[j].name, entries_sorted[j + 1].name) > 0)
            {
                struct ls_entry tmp;
                int k;
                for (k = 0; k < MAX_NAME_LEN; k++)
                    tmp.name[k] = entries_sorted[j].name[k];
                tmp.is_dir = entries_sorted[j].is_dir;
                tmp.size = entries_sorted[j].size;

                for (k = 0; k < MAX_NAME_LEN; k++)
                    entries_sorted[j].name[k] = entries_sorted[j + 1].name[k];
                entries_sorted[j].is_dir = entries_sorted[j + 1].is_dir;
                entries_sorted[j].size = entries_sorted[j + 1].size;

                for (k = 0; k < MAX_NAME_LEN; k++)
                    entries_sorted[j + 1].name[k] = tmp.name[k];
                entries_sorted[j + 1].is_dir = tmp.is_dir;
                entries_sorted[j + 1].size = tmp.size;
            }
        }
    }

    /* Print entries */
    for (i = 0; i < n_entries; i++)
    {
        if (long_fmt)
        {
            if (entries_sorted[i].is_dir)
                sys_putstr("d ");
            else
                sys_putstr("- ");
            print_num_rjust(entries_sorted[i].size, 8);
            sys_putc(' ');
        }
        sys_putstr(entries_sorted[i].name);
        if (entries_sorted[i].is_dir)
            sys_putc('/');
        sys_putc('\n');
    }

    return 0;
}
