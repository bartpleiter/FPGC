/*
 * doom_libc_bridge.c — Bridge libc FILE I/O to BDOS syscalls.
 *
 * BRFS v2 is byte-native: reads, writes, seeks and filesize are all in
 * bytes. This bridge is now a thin pass-through.
 */

#include <syscall.h>
#include <stddef.h>

/* ---- Per-fd byte cursor tracking (BDOS exposes no tell syscall) ---- */
#define MAX_OPEN_FDS 16
static int fd_byte_pos[MAX_OPEN_FDS];

/* ---- File I/O bridge ---- */

/* flags values from libc stdio.c */
#define STDIO_SWR 0x02

int _open(const char *path, int flags)
{
    int fd;

    /* For write mode, create the file if it doesn't exist */
    if (flags & STDIO_SWR) {
        fd = sys_fs_open((char *)path);
        if (fd < 0) {
            sys_fs_create((char *)path);
            fd = sys_fs_open((char *)path);
        }
    } else {
        fd = sys_fs_open((char *)path);
    }
    if (fd >= 0 && fd < MAX_OPEN_FDS) {
        fd_byte_pos[fd] = 0;
    }
    return fd;
}

int _close(int fd)
{
    if (fd >= 0 && fd < MAX_OPEN_FDS) {
        fd_byte_pos[fd] = 0;
    }
    return sys_fs_close(fd);
}

int _read(int fd, char *buf, int len)
{
    int rd;
    if (len <= 0) return 0;
    rd = sys_fs_read(fd, buf, len);
    if (rd > 0 && fd >= 0 && fd < MAX_OPEN_FDS) {
        fd_byte_pos[fd] += rd;
    }
    return rd;
}

int _lseek(int fd, int offset, int whence)
{
    int new_byte_pos;
    int sz;

    if (fd < 0 || fd >= MAX_OPEN_FDS) return -1;

    if (whence == 0) {
        new_byte_pos = offset;
    } else if (whence == 1) {
        new_byte_pos = fd_byte_pos[fd] + offset;
    } else if (whence == 2) {
        sz = sys_fs_filesize(fd);
        if (sz < 0) return -1;
        new_byte_pos = sz + offset;
    } else {
        return -1;
    }

    if (new_byte_pos < 0) new_byte_pos = 0;

    if (sys_fs_seek(fd, new_byte_pos) < 0) return -1;
    fd_byte_pos[fd] = new_byte_pos;
    return new_byte_pos;
}

int _write(int fd, const char *buf, int len)
{
    int wr;

    /* stdout/stderr → BDOS terminal + UART.
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
            sys_uart_print_str(tmp);
            offset += chunk;
        }
        return len;
    }

    if (len <= 0) return 0;
    wr = sys_fs_write(fd, (void *)buf, len);
    if (wr > 0 && fd >= 0 && fd < MAX_OPEN_FDS) {
        fd_byte_pos[fd] += wr;
    }
    return wr;
}

/* ---- Stubs for POSIX functions used by Doom ---- */

void mkdir(const char *path, int mode)
{
    sys_fs_mkdir((char *)path);
}

int _remove(const char *pathname)
{
    return sys_fs_delete((char *)pathname);
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
    old_fd = sys_fs_open((char *)oldpath);
    if (old_fd < 0)
        return -1;
    file_bytes = sys_fs_filesize(old_fd);
    if (file_bytes < 0) {
        sys_fs_close(old_fd);
        return -1;
    }

    /* Delete destination if it exists, then create it */
    sys_fs_delete((char *)newpath);
    sys_fs_create((char *)newpath);
    new_fd = sys_fs_open((char *)newpath);
    if (new_fd < 0) {
        sys_fs_close(old_fd);
        return -1;
    }

    sys_fs_seek(old_fd, 0);
    sys_fs_seek(new_fd, 0);
    bytes_left = file_bytes;
    while (bytes_left > 0) {
        int chunk = (bytes_left > (int)sizeof(buf)) ? (int)sizeof(buf) : bytes_left;
        int rd = sys_fs_read(old_fd, buf, chunk);
        if (rd <= 0)
            break;
        sys_fs_write(new_fd, buf, rd);
        bytes_left -= rd;
    }

    sys_fs_close(old_fd);
    sys_fs_close(new_fd);

    /* Delete the old file */
    sys_fs_delete((char *)oldpath);
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
