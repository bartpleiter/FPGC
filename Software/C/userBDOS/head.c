/*
 * head — print first N lines of file or stdin
 *
 * Usage: head [-n N] [file]
 * Default: 10 lines.
 */

#include <syscall.h>
#include <stdlib.h>

#define BUF_SIZE 256

int main(void)
{
    int argc;
    char **argv;
    int max_lines;
    int fd;
    int arg_idx;
    char buf[BUF_SIZE];
    int n;
    int i;
    int lines;

    argc = sys_argc();
    argv = sys_argv();

    max_lines = 10;
    arg_idx = 1;

    /* Parse -n N option */
    if (argc >= 3 && argv[1][0] == '-' && argv[1][1] == 'n')
    {
        max_lines = atoi(argv[2]);
        if (max_lines <= 0)
        {
            max_lines = 10;
        }
        arg_idx = 3;
    }

    /* Open file or use stdin */
    if (arg_idx < argc)
    {
        fd = sys_open(argv[arg_idx], O_RDONLY);
        if (fd < 0)
        {
            sys_putstr("head: ");
            sys_putstr(argv[arg_idx]);
            sys_putstr(": No such file\n");
            return 1;
        }
    }
    else
    {
        fd = 0;
    }

    lines = 0;
    while (lines < max_lines && (n = sys_read(fd, buf, BUF_SIZE)) > 0)
    {
        for (i = 0; i < n && lines < max_lines; i++)
        {
            sys_putc(buf[i]);
            if (buf[i] == '\n')
            {
                lines++;
            }
        }
    }

    if (fd != 0)
    {
        sys_close(fd);
    }
    return 0;
}
