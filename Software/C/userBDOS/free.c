/*
 * free — display free memory
 *
 * Usage: free
 * Reads /proc/meminfo and prints its contents.
 */

#include <syscall.h>

#define BUF_SIZE 256

int main(void)
{
    int fd;
    char buf[BUF_SIZE];
    int n;

    fd = sys_open("/proc/meminfo", O_RDONLY);
    if (fd < 0)
    {
        sys_putstr("free: cannot open /proc/meminfo\n");
        return 1;
    }

    while ((n = sys_read(fd, buf, BUF_SIZE)) > 0)
    {
        sys_write(1, buf, n);
    }

    sys_close(fd);
    return 0;
}
