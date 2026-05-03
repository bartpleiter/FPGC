/*
 * syscall.c — userland convenience wrappers around the BDOS syscall trap.
 *
 * The low-level syscall() function lives in syscall_asm.asm. This file
 * only exposes thin helpers; legacy wrappers (sys_term_*, sys_read_key,
 * sys_set_palette, sys_uart_print_*) were removed in shell-terminal-v2
 * Phase E — see syscall.h for the migration table.
 */

#include <syscall.h>

/* errno — required by libc stdlib (strtol etc.) */
int errno;

/*
 * _sbrk — heap allocation backing for libc malloc/free.
 * Lazily requests a heap block from BDOS via sys_heap_alloc() on first
 * call, then manages the break pointer within that block.
 */
#define SBRK_HEAP_SIZE (8 * 1024 * 1024) /* 8 MiB */
static char *sbrk_base;
static char *sbrk_ptr;

void *_sbrk(int incr)
{
    char *prev;

    if (sbrk_base == 0) {
        sbrk_base = (char *)sys_heap_alloc(SBRK_HEAP_SIZE);
        if (sbrk_base == 0)
            return (void *)-1;
        sbrk_ptr = sbrk_base;
    }
    if (sbrk_ptr + incr > sbrk_base + SBRK_HEAP_SIZE || sbrk_ptr + incr < sbrk_base)
        return (void *)-1;
    prev = sbrk_ptr;
    sbrk_ptr += incr;
    return (void *)prev;
}

/* ---- I/O ---- */

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

/* ---- Filesystem (format utilities only — raw FS wrappers removed) ---- */

int sys_fs_format(int blocks, int words_per_block, char *label)
{
    return syscall(SYSCALL_FS_FORMAT, blocks, words_per_block, (int)label);
}

int sys_sd_format(int blocks, int words_per_block, char *label)
{
    return syscall(SYSCALL_SD_FORMAT, blocks, words_per_block, (int)label);
}

/* ---- Shell ---- */

int    sys_shell_argc  (void) { return        syscall(SYSCALL_SHELL_ARGC,   0, 0, 0); }
char **sys_shell_argv  (void) { return (char **)syscall(SYSCALL_SHELL_ARGV, 0, 0, 0); }
char  *sys_shell_getcwd(void) { return (char  *)syscall(SYSCALL_SHELL_GETCWD, 0, 0, 0); }

/* ---- Heap / timing / process / key state ---- */

void *sys_heap_alloc(int size)   { return (void *)syscall(SYSCALL_HEAP_ALLOC, size, 0, 0); }
void  sys_delay     (int ms)     { syscall(SYSCALL_DELAY, ms, 0, 0); }
void  sys_exit      (int code)   { syscall(SYSCALL_EXIT,  code, 0, 0); }
void  _exit         (int code)   { syscall(SYSCALL_EXIT,  code, 0, 0); }
int   sys_get_key_state(void)    { return syscall(SYSCALL_GET_KEY_STATE, 0, 0, 0); }

/* ---- Networking ---- */

int  sys_net_send        (char *buf, int len)     { return syscall(SYSCALL_NET_SEND,         (int)buf, len, 0); }
int  sys_net_recv        (char *buf, int max_len) { return syscall(SYSCALL_NET_RECV,         (int)buf, max_len, 0); }
int  sys_net_packet_count(void)                   { return syscall(SYSCALL_NET_PACKET_COUNT, 0, 0, 0); }
void sys_net_get_mac     (int *mac_buf)           { syscall(SYSCALL_NET_GET_MAC, (int)mac_buf, 0, 0); }

/* ---- VFS / fd-oriented byte I/O ---- */

int sys_open (const char *path, int flags)         { return syscall(SYSCALL_OPEN,  (int)path, flags, 0); }
int sys_close(int fd)                              { return syscall(SYSCALL_CLOSE, fd, 0, 0); }
int sys_read (int fd, void *buf, int len)          { return syscall(SYSCALL_READ,  fd, (int)buf, len); }
int sys_write(int fd, const void *buf, int len)    { return syscall(SYSCALL_WRITE, fd, (int)buf, len); }
int sys_lseek(int fd, int offset, int whence)      { return syscall(SYSCALL_LSEEK, fd, offset, whence); }
int sys_dup2 (int oldfd, int newfd)                { return syscall(SYSCALL_DUP2,  oldfd, newfd, 0); }
int sys_unlink(const char *path)                   { return syscall(SYSCALL_UNLINK, (int)path, 0, 0); }
int sys_mkdir(const char *path)                    { return syscall(SYSCALL_MKDIR, (int)path, 0, 0); }
int sys_readdir(const char *path, void *entry_buf, int max_entries)
{
    return syscall(SYSCALL_READDIR, (int)path, (int)entry_buf, max_entries);
}

/* ---- TTY raw event helpers ---- */

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
        (void)blocking; /* fd already configured for blocking semantics */
        return -1;
    }
    return (int)( (unsigned int)buf[0]
               | ((unsigned int)buf[1] <<  8)
               | ((unsigned int)buf[2] << 16)
               | ((unsigned int)buf[3] << 24));
}
