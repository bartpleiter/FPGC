/*
 * cat — concatenate and print files
 *
 * Usage: cat [file ...]
 * With no arguments, reads from stdin.
 */

#include <syscall.h>

#define BUF_SIZE 256

int cat_fd(int fd)
{
    char buf[BUF_SIZE];
    int n;

    while ((n = sys_read(fd, buf, BUF_SIZE)) > 0)
    {
        sys_write(1, buf, n);
    }
    return (n < 0) ? 1 : 0;
}

int main(void)
{
    int argc;
    char **argv;
    int i;
    int fd;
    int ret;

    argc = sys_argc();
    argv = sys_argv();

    if (argc < 2)
    {
        return cat_fd(0);
    }

    ret = 0;
    for (i = 1; i < argc; i++)
    {
        fd = sys_open(argv[i], O_RDONLY);
        if (fd < 0)
        {
            sys_putstr("cat: ");
            sys_putstr(argv[i]);
            sys_putstr(": No such file\n");
            ret = 1;
            continue;
        }
        if (cat_fd(fd) != 0)
        {
            ret = 1;
        }
        sys_close(fd);
    }
    return ret;
}
