/*
 * ps — show process table
 *
 * Usage: ps
 * Reads /proc/ps and prints its contents.
 */

#include <syscall.h>

#define BUF_SIZE 512

int main(void)
{
    int fd;
    char buf[BUF_SIZE];
    int n;

    fd = sys_open("/proc/ps", O_RDONLY);
    if (fd < 0)
    {
        sys_putstr("ps: cannot open /proc/ps\n");
        return 1;
    }

    while ((n = sys_read(fd, buf, BUF_SIZE)) > 0)
    {
        sys_write(1, buf, n);
    }

    sys_close(fd);
    return 0;
}
