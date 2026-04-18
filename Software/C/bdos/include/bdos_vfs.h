#ifndef BDOS_VFS_H
#define BDOS_VFS_H

/*
 * Phase B of shell-terminal-v2 — virtual filesystem with byte-oriented
 * file descriptors. Three device types currently:
 *   DEV_TTY  — line-disciplined terminal (uses libterm v2 + HID FIFO).
 *   DEV_FILE — backed by BRFS, byte view over the underlying word storage.
 *   DEV_NULL — /dev/null.
 *
 * fd table is global for now (BDOS still only has one foreground program
 * at a time at the syscall level). Phase C splits it per-process.
 *
 * Path conventions:
 *   "/dev/tty"   → DEV_TTY
 *   "/dev/null"  → DEV_NULL
 *   anything else → DEV_FILE via BRFS
 */

#define BDOS_FD_MAX        16

#define BDOS_DEV_NONE      0
#define BDOS_DEV_TTY       1
#define BDOS_DEV_FILE      2
#define BDOS_DEV_NULL      3

#define BDOS_O_RDONLY      0x01
#define BDOS_O_WRONLY      0x02
#define BDOS_O_RDWR        (BDOS_O_RDONLY | BDOS_O_WRONLY)
#define BDOS_O_APPEND      0x04
#define BDOS_O_CREAT       0x08
#define BDOS_O_TRUNC       0x10

/* lseek whence values (match POSIX). */
#define BDOS_SEEK_SET      0
#define BDOS_SEEK_CUR      1
#define BDOS_SEEK_END      2

/* Reserved fds — pre-opened by bdos_vfs_init() to /dev/tty. */
#define BDOS_STDIN_FD      0
#define BDOS_STDOUT_FD     1
#define BDOS_STDERR_FD     2

typedef struct bdos_fd_s bdos_fd_t;

struct bdos_fd_s {
    int          in_use;
    int          dev;          /* BDOS_DEV_* */
    int          flags;        /* BDOS_O_*  */
    int          handle;       /* device-specific (BRFS fd, …) */
    unsigned int byte_pos;     /* byte cursor for files */

    /* Write-side byte→word accumulator for file devs. */
    unsigned int wr_buf;
    unsigned int wr_fill;      /* 0..3 bytes pending */
};

/* ---- Lifecycle ---- */
void bdos_vfs_init(void);
void bdos_vfs_shutdown(void);

/* ---- Public API (also exposed via syscalls) ---- */
int  bdos_vfs_open(const char *path, int flags);
int  bdos_vfs_close(int fd);
int  bdos_vfs_read(int fd, void *buf, int len);
int  bdos_vfs_write(int fd, const void *buf, int len);
int  bdos_vfs_lseek(int fd, int offset, int whence);

#endif /* BDOS_VFS_H */
