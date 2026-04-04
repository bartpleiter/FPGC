/*
 * syscall.c — User-side syscall convenience wrappers.
 * The low-level syscall() function is in syscall_asm.asm.
 */

#include <syscall.h>

/* errno — required by libc stdlib (strtol etc.) */
int errno;

/*
 * _sbrk — heap allocation for libc malloc/free.
 * Lazily requests a heap block from BDOS via sys_heap_alloc() on first call,
 * then manages the break pointer within that block.
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

void sys_print_char(int ch)
{
    syscall(SYSCALL_PRINT_CHAR, ch, 0, 0);
}

void sys_print_str(char *s)
{
    syscall(SYSCALL_PRINT_STR, (int)s, 0, 0);
}

int sys_read_key(void)
{
    return syscall(SYSCALL_READ_KEY, 0, 0, 0);
}

int sys_key_available(void)
{
    return syscall(SYSCALL_KEY_AVAILABLE, 0, 0, 0);
}

/* ---- Filesystem ---- */

int sys_fs_open(char *path)
{
    return syscall(SYSCALL_FS_OPEN, (int)path, 0, 0);
}

int sys_fs_close(int fd)
{
    return syscall(SYSCALL_FS_CLOSE, fd, 0, 0);
}

int sys_fs_read(int fd, void *buf, int count)
{
    return syscall(SYSCALL_FS_READ, fd, (int)buf, count);
}

int sys_fs_write(int fd, void *buf, int count)
{
    return syscall(SYSCALL_FS_WRITE, fd, (int)buf, count);
}

int sys_fs_seek(int fd, int offset)
{
    return syscall(SYSCALL_FS_SEEK, fd, offset, 0);
}

int sys_fs_stat(char *path, void *entry_buf)
{
    return syscall(SYSCALL_FS_STAT, (int)path, (int)entry_buf, 0);
}

int sys_fs_delete(char *path)
{
    return syscall(SYSCALL_FS_DELETE, (int)path, 0, 0);
}

int sys_fs_create(char *path)
{
    return syscall(SYSCALL_FS_CREATE, (int)path, 0, 0);
}

int sys_fs_filesize(int fd)
{
    return syscall(SYSCALL_FS_FILESIZE, fd, 0, 0);
}

int sys_fs_readdir(char *path, void *entry_buf, int max_entries)
{
    return syscall(SYSCALL_FS_READDIR, (int)path, (int)entry_buf, max_entries);
}

/* ---- Shell ---- */

int sys_shell_argc(void)
{
    return syscall(SYSCALL_SHELL_ARGC, 0, 0, 0);
}

char **sys_shell_argv(void)
{
    return (char **)syscall(SYSCALL_SHELL_ARGV, 0, 0, 0);
}

char *sys_shell_getcwd(void)
{
    return (char *)syscall(SYSCALL_SHELL_GETCWD, 0, 0, 0);
}

/* ---- Terminal ---- */

void sys_term_put_cell(int x, int y, int tile_palette)
{
    syscall(SYSCALL_TERM_PUT_CELL, x, y, tile_palette);
}

void sys_term_clear(void)
{
    syscall(SYSCALL_TERM_CLEAR, 0, 0, 0);
}

void sys_term_set_cursor(int x, int y)
{
    syscall(SYSCALL_TERM_SET_CURSOR, x, y, 0);
}

int sys_term_get_cursor(void)
{
    return syscall(SYSCALL_TERM_GET_CURSOR, 0, 0, 0);
}

/* ---- Heap ---- */

void *sys_heap_alloc(int size)
{
    return (void *)syscall(SYSCALL_HEAP_ALLOC, size, 0, 0);
}

/* ---- Timing ---- */

void sys_delay(int ms)
{
    syscall(SYSCALL_DELAY, ms, 0, 0);
}

/* ---- GPU ---- */

void sys_set_palette(int index, int value)
{
    syscall(SYSCALL_SET_PALETTE, index, value, 0);
}

void sys_set_pixel_palette(int index, int rgb24)
{
    syscall(SYSCALL_SET_PIXEL_PALETTE, index, rgb24, 0);
}

/* ---- Networking ---- */

int sys_net_send(char *buf, int len)
{
    return syscall(SYSCALL_NET_SEND, (int)buf, len, 0);
}

int sys_net_recv(char *buf, int max_len)
{
    return syscall(SYSCALL_NET_RECV, (int)buf, max_len, 0);
}

int sys_net_packet_count(void)
{
    return syscall(SYSCALL_NET_PACKET_COUNT, 0, 0, 0);
}

void sys_net_get_mac(int *mac_buf)
{
    syscall(SYSCALL_NET_GET_MAC, (int)mac_buf, 0, 0);
}

/* ---- UART debug output ---- */

void sys_uart_print_char(int ch)
{
    syscall(SYSCALL_UART_PRINT_CHAR, ch, 0, 0);
}

void sys_uart_print_str(char *s)
{
    syscall(SYSCALL_UART_PRINT_STR, (int)s, 0, 0);
}

/* ---- Process control ---- */

void sys_exit(int code)
{
    syscall(SYSCALL_EXIT, code, 0, 0);
}

void _exit(int code)
{
    syscall(SYSCALL_EXIT, code, 0, 0);
}

/* ---- Key state ---- */

int sys_get_key_state(void)
{
    return syscall(SYSCALL_GET_KEY_STATE, 0, 0, 0);
}
