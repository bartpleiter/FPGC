/*
 * wc — count lines, words, and bytes
 *
 * Usage: wc [file ...]
 * With no arguments, reads from stdin.
 */

#include <syscall.h>
#include <string.h>

#define BUF_SIZE 256

void print_num(int val)
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
    {
        out[j++] = tmp[--i];
    }
    out[j] = 0;
    sys_putstr(out);
}

void wc_fd(int fd, int *total_lines, int *total_words, int *total_bytes)
{
    char buf[BUF_SIZE];
    int n;
    int lines;
    int words;
    int bytes;
    int in_word;
    int i;
    char c;

    lines = 0;
    words = 0;
    bytes = 0;
    in_word = 0;

    while ((n = sys_read(fd, buf, BUF_SIZE)) > 0)
    {
        for (i = 0; i < n; i++)
        {
            c = buf[i];
            bytes++;
            if (c == '\n')
            {
                lines++;
            }
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
            {
                in_word = 0;
            }
            else
            {
                if (in_word == 0)
                {
                    words++;
                    in_word = 1;
                }
            }
        }
    }

    *total_lines += lines;
    *total_words += words;
    *total_bytes += bytes;

    sys_putstr("  ");
    print_num(lines);
    sys_putstr("  ");
    print_num(words);
    sys_putstr("  ");
    print_num(bytes);
}

int main(void)
{
    int argc;
    char **argv;
    int i;
    int fd;
    int total_lines;
    int total_words;
    int total_bytes;

    argc = sys_argc();
    argv = sys_argv();

    total_lines = 0;
    total_words = 0;
    total_bytes = 0;

    if (argc < 2)
    {
        wc_fd(0, &total_lines, &total_words, &total_bytes);
        sys_putc('\n');
        return 0;
    }

    for (i = 1; i < argc; i++)
    {
        fd = sys_open(argv[i], O_RDONLY);
        if (fd < 0)
        {
            sys_putstr("wc: ");
            sys_putstr(argv[i]);
            sys_putstr(": No such file\n");
            continue;
        }
        wc_fd(fd, &total_lines, &total_words, &total_bytes);
        sys_putstr(" ");
        sys_putstr(argv[i]);
        sys_putc('\n');
        sys_close(fd);
    }

    if (argc > 2)
    {
        sys_putstr("  ");
        print_num(total_lines);
        sys_putstr("  ");
        print_num(total_words);
        sys_putstr("  ");
        print_num(total_bytes);
        sys_putstr(" total\n");
    }

    return 0;
}
