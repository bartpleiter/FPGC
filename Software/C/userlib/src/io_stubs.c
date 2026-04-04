/*
 * io_stubs.c — Default libc I/O stubs for userBDOS programs.
 *
 * Provides _write, _read, _open, _close, _lseek implementations
 * required by stdio.c. Programs that need real file I/O (like Doom)
 * should provide their own implementations instead of linking this file.
 *
 * _write routes stdout/stderr to the BDOS terminal via sys_print_char.
 * All other operations return errors.
 */

#include <syscall.h>

int _write(int fd, const char *buf, int len)
{
    if (fd == 1 || fd == 2 || fd == -1 || fd == -2) {
        int i;
        for (i = 0; i < len; i++)
            sys_print_char(buf[i]);
        return len;
    }
    return -1;
}

int _read(int fd, char *buf, int len)
{
    return -1;
}

int _open(const char *path, int flags)
{
    return -1;
}

int _close(int fd)
{
    return -1;
}

int _lseek(int fd, int offset, int whence)
{
    return -1;
}
