/*
 * io_stubs.c — libc I/O implementation for userBDOS programs.
 *
 * Phase B: byte/word translation has moved into BDOS (vfs.c). This file
 * is now a thin shim that maps libc's negative stdin/stdout/stderr fds
 * onto the BDOS-pre-opened fds 0/1/2 and forwards everything else
 * directly to sys_open/read/write/lseek/close.
 *
 * NUL-as-EOF text-file convention: BRFS files are word-aligned, so a
 * file that ends mid-word has trailing zero bytes. Existing userBDOS
 * code relies on _read returning short on the first NUL byte. We
 * preserve that behaviour here so the byte-for-byte equivalence tests
 * (asm-link, cpp) keep passing.
 */

#include <syscall.h>

/* libc stdio uses negative fds for the standard streams */
#define STDIO_FD_STDOUT (-1)
#define STDIO_FD_STDERR (-2)
#define STDIO_FD_STDIN  (-3)

/* BDOS pre-opens these in bdos_vfs_init() */
#define BDOS_FD_STDIN   0
#define BDOS_FD_STDOUT  1
#define BDOS_FD_STDERR  2

/* stdio open flags (must match stdio.c) */
#define STDIO_SRD  1
#define STDIO_SWR  2

static int translate_fd(int fd)
{
    if (fd == STDIO_FD_STDOUT) return BDOS_FD_STDOUT;
    if (fd == STDIO_FD_STDERR) return BDOS_FD_STDERR;
    if (fd == STDIO_FD_STDIN)  return BDOS_FD_STDIN;
    return fd;
}

/*
 * _io_init — close any user file descriptors left over from previous
 * runs. The standard fds 0/1/2 are owned by BDOS and never closed here.
 */
void _io_init(void)
{
    int i;
    for (i = 3; i < 16; i++)
        sys_close(i);
}

int _write(int fd, const char *buf, int len)
{
    return sys_write(translate_fd(fd), buf, len);
}

int _read(int fd, char *buf, int len)
{
    int rfd = translate_fd(fd);
    int n = sys_read(rfd, buf, len);
    int i;

    /* Text-mode NUL-as-EOF only applies to file reads, not the tty. */
    if (n <= 0 || rfd == BDOS_FD_STDIN)
        return n;

    for (i = 0; i < n; i++) {
        if (buf[i] == 0)
            return i;
    }
    return n;
}

int _open(const char *path, int flags)
{
    int o_flags;

    if (flags & STDIO_SWR) {
        o_flags = (flags & STDIO_SRD) ? O_RDWR : O_WRONLY;
        o_flags |= O_CREAT;
    } else {
        o_flags = O_RDONLY;
    }
    return sys_open(path, o_flags);
}

int _close(int fd)
{
    return sys_close(translate_fd(fd));
}

int _lseek(int fd, int offset, int whence)
{
    return sys_lseek(translate_fd(fd), offset, whence);
}

int _remove(const char *pathname)
{
    return sys_fs_delete((char *)pathname);
}

int _rename(const char *oldpath, const char *newpath)
{
    (void)oldpath;
    (void)newpath;
    return -1;
}
