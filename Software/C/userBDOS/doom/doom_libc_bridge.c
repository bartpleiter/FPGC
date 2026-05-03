/*
 * doom_libc_bridge.c — Bridge libc FILE I/O to BDOS syscalls.
 *
 * BRFS v2 is byte-native: reads, writes, seeks and filesize are all in
 * bytes. This bridge is now a thin pass-through.
 */

#include <syscall.h>
#include <stddef.h>

/* Backward-compat shim for legacy debug-logging call sites in unmodified
 * Doom sources. Routes UART print to stderr (fd 2), which libterm mirrors
 * to UART. */
void sys_uart_print_str(const char *s)
{
    int n = 0;
    while (s[n]) n++;
    sys_write(2, (void *)s, n);
}

/* ---- Per-fd byte cursor tracking (BDOS exposes no tell syscall) ---- */
/* No longer needed — VFS sys_lseek tracks position internally. */

/* ---- File I/O bridge ---- */

/* flags values from libc stdio.c */
#define STDIO_SWR 0x02

int _open(const char *path, int flags)
{
    int fd;

    /* For write mode, open with create flag so file is created if missing */
    if (flags & STDIO_SWR) {
        fd = sys_open(path, 0x0A /* O_WRONLY | O_CREAT */);
    } else {
        fd = sys_open(path, 1 /* O_RDONLY */);
    }
    return fd;
}

int _close(int fd)
{
    return sys_close(fd);
}

int _read(int fd, char *buf, int len)
{
    if (len <= 0) return 0;
    return sys_read(fd, buf, len);
}

int _lseek(int fd, int offset, int whence)
{
    return sys_lseek(fd, offset, whence);
}

int _write(int fd, const char *buf, int len)
{
    int wr;

    /* stdout/stderr → BDOS terminal (libterm mirrors stderr to UART).
     * libc uses negative fds: stdout=-1, stderr=-2.
     * Positive fds (1, 2, ...) are valid BRFS file descriptors. */
    if (fd == -1 || fd == -2) {
        char tmp[129];
        int offset = 0;
        while (offset < len) {
            int chunk = len - offset;
            if (chunk > 128) chunk = 128;
            int j;
            for (j = 0; j < chunk; j++)
                tmp[j] = buf[offset + j];
            tmp[chunk] = '\0';
            sys_putstr(tmp);
            sys_write(2, tmp, chunk);
            offset += chunk;
        }
        return len;
    }

    if (len <= 0) return 0;
    wr = sys_write(fd, (void *)buf, len);
    return wr;
}

/* ---- Stubs for POSIX functions used by Doom ---- */

void mkdir(const char *path, int mode)
{
    sys_mkdir(path);
}

int _remove(const char *pathname)
{
    return sys_unlink(pathname);
}

/*
 * _rename — rename a file by copy + delete.
 * BRFS has no native rename, so we read the old file, create the new
 * file, write the data, then delete the old file.
 */
int _rename(const char *oldpath, const char *newpath)
{
    int old_fd;
    int new_fd;
    int file_bytes;
    unsigned char buf[512];
    int bytes_left;

    /* Open old file and get its size */
    old_fd = sys_open(oldpath, 1 /* O_RDONLY */);
    if (old_fd < 0)
        return -1;
    file_bytes = sys_lseek(old_fd, 0, 2 /* SEEK_END */);
    if (file_bytes < 0) {
        sys_close(old_fd);
        return -1;
    }
    sys_lseek(old_fd, 0, 0 /* SEEK_SET */);

    /* Create/truncate destination */
    new_fd = sys_open(newpath, 0x1A /* O_WRONLY | O_CREAT | O_TRUNC */);
    if (new_fd < 0) {
        sys_close(old_fd);
        return -1;
    }

    bytes_left = file_bytes;
    while (bytes_left > 0) {
        int chunk = (bytes_left > (int)sizeof(buf)) ? (int)sizeof(buf) : bytes_left;
        int rd = sys_read(old_fd, buf, chunk);
        if (rd <= 0)
            break;
        sys_write(new_fd, buf, rd);
        bytes_left -= rd;
    }

    sys_close(old_fd);
    sys_close(new_fd);

    /* Delete the old file */
    sys_unlink(oldpath);
    return 0;
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
