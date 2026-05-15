/*
 * cp — copy file
 *
 * Usage: cp <source> <dest>
 */

#include <syscall.h>

#define BUF_SIZE 256

int main(void)
{
    int argc;
    char **argv;
    int src_fd;
    int dst_fd;
    char buf[BUF_SIZE];
    int n;

    argc = sys_argc();
    argv = sys_argv();

    if (argc != 3)
    {
        sys_putstr("usage: cp <source> <dest>\n");
        return 1;
    }

    src_fd = sys_open(argv[1], O_RDONLY);
    if (src_fd < 0)
    {
        sys_putstr("cp: cannot open '");
        sys_putstr(argv[1]);
        sys_putstr("'\n");
        return 1;
    }

    dst_fd = sys_open(argv[2], O_WRONLY | O_CREAT | O_TRUNC);
    if (dst_fd < 0)
    {
        sys_putstr("cp: cannot create '");
        sys_putstr(argv[2]);
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
