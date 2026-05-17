/*
 * df — display filesystem disk space usage
 *
 * Usage: df
 * Reads /proc/df and prints its contents.
 */

#include <syscall.h>

#define BUF_SIZE 512

int main(void)
{
    int fd;
    char buf[BUF_SIZE];
    int n;

    fd = sys_open("/proc/df", O_RDONLY);
    if (fd < 0)
    {
        sys_putstr("df: cannot open /proc/df\n");
        return 1;
    }

    while ((n = sys_read(fd, buf, BUF_SIZE)) > 0)
    {
        sys_write(1, buf, n);
    }

    sys_close(fd);
    return 0;
}
