/*
 * syscall.c — BDOS v4 userland syscall wrappers.
 *
 * The low-level syscall() function lives in syscall_asm.asm.
 * This file provides thin C wrappers and the _sbrk backing for malloc.
 */

#include <syscall.h>

/* errno — required by libc stdlib (strtol etc.) */
int errno;

/*
 * _sbrk — heap allocation backing for libc malloc/free.
 * Calls the kernel SBRK syscall to extend the process heap.
 */
void *_sbrk(int incr)
{
    return sys_sbrk(incr);
}

/* ---- Process control ---- */

void sys_exit (int code) { syscall(SYS_EXIT,  code, 0, 0); }
void _exit    (int code) { syscall(SYS_EXIT,  code, 0, 0); }
int  sys_getpid(void)    { return syscall(SYS_GETPID, 0, 0, 0); }
void sys_yield(void)     { syscall(SYS_YIELD, 0, 0, 0); }

/* ---- I/O convenience ---- */

void sys_putc(int ch)
{
    char c = (char)ch;
    sys_write(1, &c, 1);
}

void sys_putstr(const char *s)
{
    int n = 0;
    while (s[n]) n++;
    sys_write(1, s, n);
}

/* ---- File I/O ---- */

int sys_open (const char *path, int flags)         { return syscall(SYS_OPEN,  (int)path, flags, 0); }
int sys_close(int fd)                              { return syscall(SYS_CLOSE, fd, 0, 0); }
int sys_read (int fd, void *buf, int len)          { return syscall(SYS_READ,  fd, (int)buf, len); }
int sys_write(int fd, const void *buf, int len)    { return syscall(SYS_WRITE, fd, (int)buf, len); }
int sys_lseek(int fd, int offset, int whence)      { return syscall(SYS_LSEEK, fd, offset, whence); }
int sys_dup2 (int oldfd, int newfd)                { return syscall(SYS_DUP2,  oldfd, newfd, 0); }

/* ---- Filesystem ---- */

int sys_unlink(const char *path)                   { return syscall(SYS_UNLINK, (int)path, 0, 0); }
int sys_mkdir(const char *path)                    { return syscall(SYS_MKDIR, (int)path, 0, 0); }
int sys_readdir(const char *path, void *entry_buf, int max_entries)
{
    return syscall(SYS_READDIR, (int)path, (int)entry_buf, max_entries);
}
int sys_rename(const char *oldpath, const char *newpath)
{
    return syscall(SYS_RENAME, (int)oldpath, (int)newpath, 0);
}
int sys_sync(void) { return syscall(SYS_SYNC, 0, 0, 0); }

int sys_fs_format(int blocks, int words_per_block, char *label)
{
    return syscall(SYS_FORMAT, blocks, words_per_block, (int)label);
}

int sys_sd_format(int blocks, int words_per_block, char *label)
{
    return syscall(SYS_SD_FORMAT, blocks, words_per_block, (int)label);
}

/* ---- Process environment ---- */

int    sys_argc(void)             { return syscall(SYS_ARGC, 0, 0, 0); }
char **sys_argv(void)             { return (char **)syscall(SYS_ARGV, 0, 0, 0); }
int    sys_getcwd(char *buf, int size) { return syscall(SYS_GETCWD, (int)buf, size, 0); }
int    sys_chdir(const char *path)     { return syscall(SYS_CHDIR, (int)path, 0, 0); }
void  *sys_sbrk(int incr)             { return (void *)syscall(SYS_SBRK, incr, 0, 0); }

/* ---- Timing ---- */

void sys_sleep(int ms)           { syscall(SYS_SLEEP, ms, 0, 0); }
int  sys_get_time_us(void)       { return syscall(SYS_GET_TIME_US, 0, 0, 0); }

/* ---- Input ---- */

int sys_get_key_state(void)      { return syscall(SYS_GET_KEY_STATE, 0, 0, 0); }

/* ---- Networking ---- */

int  sys_net_send        (char *buf, int len)     { return syscall(SYS_NET_SEND,         (int)buf, len, 0); }
int  sys_net_recv        (char *buf, int max_len) { return syscall(SYS_NET_RECV,         (int)buf, max_len, 0); }
int  sys_net_packet_count(void)                   { return syscall(SYS_NET_PACKET_COUNT, 0, 0, 0); }
void sys_net_get_mac     (int *mac_buf)           { syscall(SYS_NET_GET_MAC, (int)mac_buf, 0, 0); }

/* ---- TTY event helpers ---- */

int sys_tty_open_raw(int nonblocking)
{
    int flags = O_RDONLY | O_RAW;
    if (nonblocking) flags |= O_NONBLOCK;
    return sys_open("/dev/tty", flags);
}

int sys_tty_event_read(int fd, int blocking)
{
    unsigned char buf[4];
    int n = sys_read(fd, buf, 4);
    if (n != 4) {
        (void)blocking;
        return -1;
    }
    return (int)( (unsigned int)buf[0]
               | ((unsigned int)buf[1] <<  8)
               | ((unsigned int)buf[2] << 16)
               | ((unsigned int)buf[3] << 24));
}
