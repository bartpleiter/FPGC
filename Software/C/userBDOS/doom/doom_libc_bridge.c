/*
 * doom_libc_bridge.c — Bridge libc FILE I/O to BDOS syscalls.
 *
 * The FPGC libc has fopen/fread/fseek/fclose, but their backing
 * system calls (_open, _read, _lseek, _close) are stubs that return
 * errors. This file provides real implementations using BDOS syscalls.
 *
 * _sbrk is provided by userlib/src/syscall.c (heap bootstrap via
 * sys_heap_alloc).
 */

#include <syscall.h>
#include <stddef.h>

/* ---- File I/O bridge ---- */

int _open(const char *path, int flags)
{
    int fd = sys_fs_open((char *)path);
    sys_uart_print_str("_open(\"");
    sys_uart_print_str((char *)path);
    sys_uart_print_str("\") = ");
    /* print fd as decimal */
    if (fd < 0) {
        sys_uart_print_str("-1");
    } else {
        char buf[12];
        int i = 11;
        int n = fd;
        buf[i] = 0;
        if (n == 0) buf[--i] = '0';
        while (n > 0) { buf[--i] = '0' + (n % 10); n /= 10; }
        sys_uart_print_str(&buf[i]);
    }
    sys_uart_print_str("\n");
    return fd;
}

int _close(int fd)
{
    return sys_fs_close(fd);
}

int _read(int fd, char *buf, int len)
{
    return sys_fs_read(fd, buf, len);
}

/*
 * _lseek — seek to position in file.
 * BDOS sys_fs_seek only supports absolute offset (SEEK_SET).
 * We approximate SEEK_CUR/SEEK_END here.
 */
int _lseek(int fd, int offset, int whence)
{
    if (whence == 0) {
        /* SEEK_SET */
        return sys_fs_seek(fd, offset);
    } else if (whence == 1) {
        /* SEEK_CUR — not directly supported, assume current position tracking
         * is handled by the libc's ftell. For now, this is best-effort. */
        return sys_fs_seek(fd, offset);
    } else if (whence == 2) {
        /* SEEK_END */
        int size = sys_fs_filesize(fd);
        if (size < 0) return -1;
        return sys_fs_seek(fd, size + offset);
    }
    return -1;
}

int _write(int fd, const char *buf, int len)
{
    /* stdout/stderr → print to terminal AND UART for debugging */
    if (fd == 1 || fd == 2 || fd == -1 || fd == -2) {
        int i;
        for (i = 0; i < len; i++) {
            sys_print_char(buf[i]);
            sys_uart_print_char(buf[i]);
        }
        return len;
    }
    /* File write */
    return sys_fs_write(fd, (void *)buf, len);
}

/* ---- Stubs for POSIX functions used by Doom ---- */

void mkdir(const char *path, int mode)
{
    /* BRFS doesn't have directories — no-op */
}

/* ---- String utilities ---- */

int strcasecmp(const char *s1, const char *s2)
{
    while (*s1 && *s2) {
        int c1 = *s1;
        int c2 = *s2;
        if (c1 >= 'A' && c1 <= 'Z') c1 += 32;
        if (c2 >= 'A' && c2 <= 'Z') c2 += 32;
        if (c1 != c2) return c1 - c2;
        s1++;
        s2++;
    }
    return (unsigned char)*s1 - (unsigned char)*s2;
}

int strncasecmp(const char *s1, const char *s2, unsigned int n)
{
    while (n-- > 0 && *s1 && *s2) {
        int c1 = *s1;
        int c2 = *s2;
        if (c1 >= 'A' && c1 <= 'Z') c1 += 32;
        if (c2 >= 'A' && c2 <= 'Z') c2 += 32;
        if (c1 != c2) return c1 - c2;
        s1++;
        s2++;
    }
    if (n == (unsigned int)-1) return 0;
    return (unsigned char)*s1 - (unsigned char)*s2;
}
