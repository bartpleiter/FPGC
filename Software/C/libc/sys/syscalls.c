#include <stddef.h>
#include <errno.h>

/* Global errno — defined here, declared extern in errno.h and stdio.c */
int errno;

/* UART memory-mapped I/O addresses */
#define UART_TX_ADDR  0x1C000000
#define UART_RX_ADDR  0x1C000004

/*========================================================================
 * _write — write bytes to a file descriptor
 *
 * fd 1 (stdout) and 2 (stderr) go to UART TX.
 * Also handles the internal negative fd values from stdio.c:
 *   -1 = stdout, -2 = stderr
 *======================================================================*/
int
_write(int fd, const char *buf, int len)
{
    int i;

    if (fd == 1 || fd == 2 || fd == -1 || fd == -2) {
        for (i = 0; i < len; i++)
            __builtin_store(UART_TX_ADDR, (int)(unsigned char)buf[i]);
        return len;
    }

    errno = EBADF;
    return -1;
}

/*========================================================================
 * _read — read bytes from a file descriptor
 *
 * fd 0 (stdin) / -3 reads from UART RX (blocking, one byte at a time).
 * NOTE: bare metal UART RX is interrupt-driven in the full BDOS.
 *       This stub does a simple polling read — blocking!
 *======================================================================*/
int
_read(int fd, char *buf, int len)
{
    int i;

    if (fd == 0 || fd == -3) {
        for (i = 0; i < len; i++)
            buf[i] = (char)__builtin_load(UART_RX_ADDR);
        return len;
    }

    errno = EBADF;
    return -1;
}

/*========================================================================
 * _sbrk — grow the heap by `incr` bytes
 *
 * Bare metal: heap lives at 0x400000 — 0x1DFFFFC (below stack).
 * BDOS kernel: heap at MEM_HEAP_START — MEM_HEAP_END.
 *
 * The linker doesn't emit an _end symbol yet, so we hardcode.
 * TODO: once the linker supports it, use _end as heap_start.
 *======================================================================*/
#define HEAP_START  ((char *)0x400000)
#define HEAP_END    ((char *)0x1DFFFFC)

static char *heap_ptr = HEAP_START;

void *
_sbrk(int incr)
{
    char *prev = heap_ptr;

    if (heap_ptr + incr > HEAP_END || heap_ptr + incr < HEAP_START) {
        errno = ENOMEM;
        return (void *)-1;
    }

    heap_ptr += incr;
    return prev;
}

/*========================================================================
 * _open / _close / _lseek — file operation stubs
 *
 * Bare metal has no filesystem. These return errors.
 * For BDOS kernel mode, replace with BRFS wrappers.
 *======================================================================*/
int
_open(const char *path, int flags)
{
    (void)path;
    (void)flags;
    errno = ENOENT;
    return -1;
}

int
_close(int fd)
{
    (void)fd;
    return 0;
}

int
_lseek(int fd, int offset, int whence)
{
    (void)fd;
    (void)offset;
    (void)whence;
    errno = EBADF;
    return -1;
}

int
_remove(const char *pathname)
{
    (void)pathname;
    return -1;
}

int
_rename(const char *oldpath, const char *newpath)
{
    (void)oldpath;
    (void)newpath;
    return -1;
}
