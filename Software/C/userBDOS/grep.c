/*
 * grep — search for pattern in files or stdin
 *
 * Usage: grep <pattern> [file ...]
 * Prints lines containing the pattern.
 */

#include <syscall.h>
#include <string.h>

#define BUF_SIZE  256
#define LINE_SIZE 512

int grep_fd(int fd, const char *pattern, const char *filename, int show_name)
{
    char buf[BUF_SIZE];
    char line[LINE_SIZE];
    int line_len;
    int n;
    int i;
    int found;

    line_len = 0;
    found = 0;

    while ((n = sys_read(fd, buf, BUF_SIZE)) > 0)
    {
        for (i = 0; i < n; i++)
        {
            if (buf[i] == '\n' || line_len >= LINE_SIZE - 1)
            {
                line[line_len] = 0;

                if (strstr(line, pattern) != 0)
                {
                    found = 1;
                    if (show_name)
                    {
                        sys_putstr(filename);
                        sys_putc(':');
                    }
                    sys_putstr(line);
                    sys_putc('\n');
                }
                line_len = 0;
            }
            else
            {
                line[line_len++] = buf[i];
            }
        }
    }

    /* Handle last line without trailing newline */
    if (line_len > 0)
    {
        line[line_len] = 0;
        if (strstr(line, pattern) != 0)
        {
            found = 1;
            if (show_name)
            {
                sys_putstr(filename);
                sys_putc(':');
            }
            sys_putstr(line);
            sys_putc('\n');
        }
    }

    return found;
}

int main(void)
{
    int argc;
    char **argv;
    int i;
    int fd;
    int show_name;
    int found;

    argc = sys_argc();
    argv = sys_argv();

    if (argc < 2)
    {
        sys_putstr("usage: grep <pattern> [file ...]\n");
        return 1;
    }

    show_name = (argc > 3) ? 1 : 0;
    found = 0;

    if (argc == 2)
    {
        /* Read from stdin */
        if (grep_fd(0, argv[1], "", 0))
        {
            found = 1;
        }
    }
    else
    {
        for (i = 2; i < argc; i++)
        {
            fd = sys_open(argv[i], O_RDONLY);
            if (fd < 0)
            {
                sys_putstr("grep: ");
                sys_putstr(argv[i]);
                sys_putstr(": No such file\n");
                continue;
            }
            if (grep_fd(fd, argv[1], argv[i], show_name))
            {
                found = 1;
            }
            sys_close(fd);
        }
    }

    return found ? 0 : 1;
}
