/*
 * vfs.c — virtual filesystem layer for BDOS.
 *
 * See bdos_vfs.h for the design.
 *
 * Devices live behind a single dev_ops vtable. The fd table is global
 * (single foreground process); per-process fds will be added in Phase C.
 *
 * Byte semantics over BRFS:
 *   - Read: brfs_read returns whole words. We stash leftover bytes in
 *     the fd's wr_buf? No — for reads we just re-issue word-aligned
 *     reads each call (fast enough for our toolchain, and simpler than
 *     stateful read buffering).
 *   - Write: bytes accumulate into wr_buf. When 4 bytes are pending,
 *     a whole word is committed via brfs_write. Partial trailing word
 *     is flushed (zero-padded) on close.
 *   - Mid-file writes (writes that aren't append-only) require a
 *     read-modify-write at the start word; we handle this by issuing
 *     a brfs_read of the partial word before merging.
 */

#ifdef VFS_HOST_TEST
#include "vfs_host_stubs.h"
#else
#include "bdos.h"
#include "bdos_vfs.h"
#include "bdos_proc.h"
#endif

#ifndef NULL
#define NULL ((void *)0)
#endif

/* ============================================================ State =====
 *
 * Phase C: the fd table now lives inside the current process's
 * bdos_proc_t. The VFS just routes through bdos_proc_current()->fds[].
 * For Phase B compatibility, when no proc has been initialised yet
 * (early boot, before bdos_proc_init runs), we fall back to a single
 * static table so any pre-init code paths don't crash.
 */

static bdos_fd_t g_boot_fds[BDOS_FD_MAX];
static int       g_use_boot_fds = 1;

static bdos_fd_t *fds(void)
{
    if (g_use_boot_fds)
        return g_boot_fds;
    return bdos_proc_current()->fds;
}

/* Forward declarations of per-device operations */
static int tty_read (bdos_fd_t *f, void *buf, int len);
static int tty_write(bdos_fd_t *f, const void *buf, int len);
static int tty_close(bdos_fd_t *f);
static int tty_lseek(bdos_fd_t *f, int off, int whence);

static int file_read (bdos_fd_t *f, void *buf, int len);
static int file_write(bdos_fd_t *f, const void *buf, int len);
static int file_close(bdos_fd_t *f);
static int file_lseek(bdos_fd_t *f, int off, int whence);

static int null_read (bdos_fd_t *f, void *buf, int len);
static int null_write(bdos_fd_t *f, const void *buf, int len);
static int null_close(bdos_fd_t *f);
static int null_lseek(bdos_fd_t *f, int off, int whence);

static int pixpal_read (bdos_fd_t *f, void *buf, int len);
static int pixpal_write(bdos_fd_t *f, const void *buf, int len);
static int pixpal_close(bdos_fd_t *f);
static int pixpal_lseek(bdos_fd_t *f, int off, int whence);

static int sdfile_read (bdos_fd_t *f, void *buf, int len);
static int sdfile_write(bdos_fd_t *f, const void *buf, int len);
static int sdfile_close(bdos_fd_t *f);
static int sdfile_lseek(bdos_fd_t *f, int off, int whence);

typedef struct {
    int (*read )(bdos_fd_t *, void *, int);
    int (*write)(bdos_fd_t *, const void *, int);
    int (*close)(bdos_fd_t *);
    int (*lseek)(bdos_fd_t *, int, int);
} dev_ops_t;

static const dev_ops_t dev_table[] = {
    { NULL,        NULL,         NULL,         NULL         }, /* DEV_NONE — sentinel */
    { tty_read,    tty_write,    tty_close,    tty_lseek    }, /* DEV_TTY    */
    { file_read,   file_write,   file_close,   file_lseek   }, /* DEV_FILE   */
    { null_read,   null_write,   null_close,   null_lseek   }, /* DEV_NULL   */
    { pixpal_read, pixpal_write, pixpal_close, pixpal_lseek }, /* DEV_PIXPAL */
    { sdfile_read, sdfile_write, sdfile_close, sdfile_lseek }, /* DEV_SDFILE */
};
#define DEV_TABLE_LEN ((int)(sizeof(dev_table) / sizeof(dev_table[0])))

/* ============================================================ Helpers ==== */

static int alloc_fd(void)
{
    bdos_fd_t *t = fds();
    int i;
    for (i = 0; i < BDOS_FD_MAX; i++) {
        if (!t[i].in_use)
            return i;
    }
    return -1;
}

static bdos_fd_t *get_fd(int fd)
{
    bdos_fd_t *t = fds();
    if (fd < 0 || fd >= BDOS_FD_MAX) return NULL;
    if (!t[fd].in_use)                return NULL;
    return &t[fd];
}

static int str_eq(const char *a, const char *b)
{
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

static int str_startswith(const char *s, const char *prefix)
{
    while (*prefix) {
        if (*s != *prefix) return 0;
        s++;
        prefix++;
    }
    return 1;
}

/* ============================================================ TTY dev === */

/*
 * Reads from /dev/tty. Two modes:
 *
 *   Cooked (default): each call returns ASCII bytes from the keyboard
 *   FIFO. Special keys (>= 0x100) are dropped silently. Blocks until
 *   at least one byte arrives. Used by libc stdio (printf/getchar/...)
 *   and by the shell line editor.
 *
 *   Raw (BDOS_O_RAW): each call returns 4-byte little-endian event
 *   packets, one per FIFO event, including special keys (arrows,
 *   F-keys, ...). `len` should be a multiple of 4. With BDOS_O_NONBLOCK
 *   set, returns 0 immediately if the FIFO is empty; otherwise blocks
 *   until at least one event is available. This is the replacement
 *   for the retired SYSCALL_READ_KEY / SYSCALL_KEY_AVAILABLE.
 */
static int tty_read(bdos_fd_t *f, void *buf, int len)
{
    char *out = (char *)buf;
    int n = 0;
    int ev;

    if (len <= 0) return 0;

    if (f->flags & BDOS_O_RAW) {
        int max_events = len / 4;
        int got = 0;
        if (max_events == 0) return 0;

        /* Block (or not) for first event */
        for (;;) {
            if (bdos_keyboard_event_available()) break;
            if (f->flags & BDOS_O_NONBLOCK) return 0;
        }

        while (got < max_events) {
            if (!bdos_keyboard_event_available()) break;
            ev = bdos_keyboard_event_read();
            if (ev < 0) break;
            out[got * 4 + 0] = (char)( (unsigned int)ev        & 0xFF);
            out[got * 4 + 1] = (char)(((unsigned int)ev >>  8) & 0xFF);
            out[got * 4 + 2] = (char)(((unsigned int)ev >> 16) & 0xFF);
            out[got * 4 + 3] = (char)(((unsigned int)ev >> 24) & 0xFF);
            got++;
        }
        return got * 4;
    }

    /* Cooked mode: ASCII pass-through */
    while (n == 0) {
        if (!bdos_keyboard_event_available())
            continue;
        ev = bdos_keyboard_event_read();
        if (ev < 0)             continue;
        if (ev >= 0x100)        continue; /* drop special keys */
        out[n++] = (char)ev;
    }

    /* Drain any further pending bytes without blocking */
    while (n < len && bdos_keyboard_event_available()) {
        ev = bdos_keyboard_event_read();
        if (ev < 0)      continue;
        if (ev >= 0x100) continue;
        out[n++] = (char)ev;
    }
    return n;
}

static int tty_write(bdos_fd_t *f, const void *buf, int len)
{
    const char *p = (const char *)buf;
    int i;
    (void)f;
    for (i = 0; i < len; i++)
        term_putchar(p[i]);
    return len;
}

static int tty_close(bdos_fd_t *f) { (void)f; return 0; }
static int tty_lseek(bdos_fd_t *f, int off, int whence)
{ (void)f; (void)off; (void)whence; return -1; }

/* ============================================================ File dev ==
 *
 * BRFS v2 is byte-native: file_* ops are direct pass-throughs.
 * The byteview byte→word shim used in v1 is gone (BRFS-v2 Phase 3).
 */

static int file_read(bdos_fd_t *f, void *buf, int len)
{
    if (len <= 0) return 0;
    return brfs_read(&brfs_spi, f->handle, buf, (unsigned int)len);
}

static int file_write(bdos_fd_t *f, const void *buf, int len)
{
    if (len <= 0) return 0;
    return brfs_write(&brfs_spi, f->handle, buf, (unsigned int)len);
}

static int file_close(bdos_fd_t *f)
{
    return brfs_close(&brfs_spi, f->handle);
}

static int file_lseek(bdos_fd_t *f, int off, int whence)
{
    int newpos;
    int cur;
    int sz;

    switch (whence) {
        case BDOS_SEEK_SET:
            newpos = off;
            break;
        case BDOS_SEEK_CUR:
            cur = brfs_tell(&brfs_spi, f->handle);
            if (cur < 0) return -1;
            newpos = cur + off;
            break;
        case BDOS_SEEK_END:
            sz = brfs_file_size(&brfs_spi, f->handle);
            if (sz < 0) return -1;
            newpos = sz + off;
            break;
        default:
            return -1;
    }
    if (newpos < 0) newpos = 0;
    return brfs_seek(&brfs_spi, f->handle, (unsigned int)newpos);
}

/* ============================================================ Null dev == */

static int null_read (bdos_fd_t *f, void *buf, int len)
{ (void)f; (void)buf; (void)len; return 0; }

static int null_write(bdos_fd_t *f, const void *buf, int len)
{ (void)f; (void)buf; return len; }

static int null_close(bdos_fd_t *f) { (void)f; return 0; }
static int null_lseek(bdos_fd_t *f, int off, int whence)
{ (void)f; (void)off; (void)whence; return 0; }

/* ============================================================ Pixpal ===
 *
 * /dev/pixpal — 256-entry × 4-byte 8-bit pixel-palette DAC.
 *
 * Mirrors the MS-DOS / VGA DAC autoincrement-on-write model:
 *   open ("/dev/pixpal", O_WRONLY)
 *   lseek(fd, idx * 4, SEEK_SET)
 *   write(fd, &rgb24, 4)            -> one entry, autoincrements
 *   write(fd, batch,   256 * 4)     -> full reload
 *
 * Each 4-byte record is 0x00RRGGBB little-endian. Top byte is ignored
 * on write and returned as 0 on read. Both `len` and the current
 * cursor must be 4-byte aligned (returns -1 otherwise). Cursor is
 * stored in f->handle.
 */

#define PIXPAL_SIZE_BYTES 1024  /* 256 entries * 4 bytes */

static int pixpal_read(bdos_fd_t *f, void *buf, int len)
{
    unsigned int *out = (unsigned int *)buf;
    int cursor = f->handle;
    int i;

    if (len <= 0) return 0;
    if ((len & 3) || (cursor & 3)) return -1;
    if (cursor >= PIXPAL_SIZE_BYTES) return 0;
    if (cursor + len > PIXPAL_SIZE_BYTES)
        len = PIXPAL_SIZE_BYTES - cursor;

    for (i = 0; i < len; i += 4) {
        unsigned int idx = (unsigned int)(cursor + i) >> 2;
        out[i >> 2] = gpu_get_pixel_palette(idx);
    }
    f->handle = cursor + len;
    return len;
}

static int pixpal_write(bdos_fd_t *f, const void *buf, int len)
{
    const unsigned int *in = (const unsigned int *)buf;
    int cursor = f->handle;
    int i;

    if (len <= 0) return 0;
    if ((len & 3) || (cursor & 3)) return -1;
    if (cursor + len > PIXPAL_SIZE_BYTES) return -1;

    for (i = 0; i < len; i += 4) {
        unsigned int idx = (unsigned int)(cursor + i) >> 2;
        gpu_set_pixel_palette(idx, in[i >> 2] & 0x00FFFFFF);
    }
    f->handle = cursor + len;
    return len;
}

static int pixpal_close(bdos_fd_t *f) { (void)f; return 0; }

static int pixpal_lseek(bdos_fd_t *f, int off, int whence)
{
    int newpos;
    switch (whence) {
        case BDOS_SEEK_SET: newpos = off; break;
        case BDOS_SEEK_CUR: newpos = f->handle + off; break;
        case BDOS_SEEK_END: newpos = PIXPAL_SIZE_BYTES + off; break;
        default: return -1;
    }
    if (newpos < 0 || newpos > PIXPAL_SIZE_BYTES) return -1;
    f->handle = newpos;
    return newpos;
}

/* ============================================================ SD file dev =
 *
 * DEV_SDFILE — files on the SD card BRFS instance (brfs_sd).
 * Same pass-through approach as DEV_FILE but using brfs_sd instead of brfs_spi.
 */

static int sdfile_read(bdos_fd_t *f, void *buf, int len)
{
    if (len <= 0) return 0;
    return brfs_read(&brfs_sd, f->handle, buf, (unsigned int)len);
}

static int sdfile_write(bdos_fd_t *f, const void *buf, int len)
{
    if (len <= 0) return 0;
    return brfs_write(&brfs_sd, f->handle, buf, (unsigned int)len);
}

static int sdfile_close(bdos_fd_t *f)
{
    return brfs_close(&brfs_sd, f->handle);
}

static int sdfile_lseek(bdos_fd_t *f, int off, int whence)
{
    int newpos;
    int cur;
    int sz;

    switch (whence) {
        case BDOS_SEEK_SET:
            newpos = off;
            break;
        case BDOS_SEEK_CUR:
            cur = brfs_tell(&brfs_sd, f->handle);
            if (cur < 0) return -1;
            newpos = cur + off;
            break;
        case BDOS_SEEK_END:
            sz = brfs_file_size(&brfs_sd, f->handle);
            if (sz < 0) return -1;
            newpos = sz + off;
            break;
        default:
            return -1;
    }
    if (newpos < 0) newpos = 0;
    return brfs_seek(&brfs_sd, f->handle, (unsigned int)newpos);
}

/* ============================================================ Public ==== */

void bdos_vfs_init(void)
{
    int i;
    bdos_fd_t *t = fds();
    for (i = 0; i < BDOS_FD_MAX; i++) {
        t[i].in_use = 0;
    }

    /* Pre-open stdin (0), stdout (1), stderr (2) → /dev/tty. */
    for (i = 0; i < 3; i++) {
        t[i].in_use   = 1;
        t[i].dev      = BDOS_DEV_TTY;
        t[i].flags    = (i == 0) ? BDOS_O_RDONLY : BDOS_O_WRONLY;
        t[i].handle   = 0;
    }
}

void bdos_vfs_use_proc_tables(void)
{
    g_use_boot_fds = 0;
}

void bdos_vfs_shutdown(void)
{
    int i;
    for (i = 3; i < BDOS_FD_MAX; i++) {
        bdos_fd_t *t = fds();
        if (t[i].in_use)
            bdos_vfs_close(i);
    }
}

int bdos_vfs_open(const char *path, int flags)
{
    int fd;
    bdos_fd_t *f;
    int dev;
    int handle = 0;

    if (!path) return -1;

    if (str_eq(path, "/dev/tty")) {
        dev = BDOS_DEV_TTY;
    } else if (str_eq(path, "/dev/null")) {
        dev = BDOS_DEV_NULL;
    } else if (str_eq(path, "/dev/pixpal")) {
        /* Reject create/truncate/append — fixed-size MMIO device. */
        if (flags & (BDOS_O_CREAT | BDOS_O_TRUNC | BDOS_O_APPEND))
            return -1;
        dev = BDOS_DEV_PIXPAL;
        handle = 0;  /* byte cursor starts at 0 */
    } else if (str_startswith(path, "/sdcard/") || str_eq(path, "/sdcard")) {
        if (!bdos_sd_ready) return -1;
        dev = BDOS_DEV_SDFILE;
        /* Strip "/sdcard" prefix: "/sdcard/foo" → "/foo" */
        {
            const char *sd_path = path + 7;  /* skip "/sdcard" */
            if (*sd_path == '\0') sd_path = "/";
            if ((flags & BDOS_O_TRUNC) && (flags & BDOS_O_WRONLY)) {
                if (brfs_exists(&brfs_sd, sd_path) > 0) brfs_delete(&brfs_sd, sd_path);
            }
            handle = brfs_open(&brfs_sd, sd_path);
            if (handle < 0 && (flags & BDOS_O_CREAT)) {
                if (brfs_create_file(&brfs_sd, sd_path) < 0) return -1;
                handle = brfs_open(&brfs_sd, sd_path);
            }
            if (handle < 0) return -1;
        }
    } else {
        dev = BDOS_DEV_FILE;
        /* Honor O_TRUNC: if the file exists and the caller asked to
         * truncate, delete it first so the recreation below starts from
         * an empty file. (BRFS has no in-place truncate.) */
        if ((flags & BDOS_O_TRUNC) && (flags & BDOS_O_WRONLY)) {
            if (brfs_exists(&brfs_spi, path) > 0) brfs_delete(&brfs_spi, path);
        }
        handle = brfs_open(&brfs_spi, path);
        if (handle < 0 && (flags & BDOS_O_CREAT)) {
            if (brfs_create_file(&brfs_spi, path) < 0) return -1;
            handle = brfs_open(&brfs_spi, path);
        }
        if (handle < 0) return -1;
    }

    fd = alloc_fd();
    if (fd < 0) {
        if (dev == BDOS_DEV_FILE) brfs_close(&brfs_spi, handle);
        if (dev == BDOS_DEV_SDFILE) brfs_close(&brfs_sd, handle);
        return -1;
    }
    f = &fds()[fd];
    f->in_use   = 1;
    f->dev      = dev;
    f->flags    = flags;
    f->handle   = handle;
    return fd;
}

int bdos_vfs_close(int fd)
{
    bdos_fd_t *f = get_fd(fd);
    int rc;
    if (!f) return -1;
    rc = dev_table[f->dev].close(f);
    f->in_use = 0;
    f->dev    = BDOS_DEV_NONE;
    return rc;
}

int bdos_vfs_read(int fd, void *buf, int len)
{
    bdos_fd_t *f = get_fd(fd);
    if (!f) return -1;
    if (!(f->flags & BDOS_O_RDONLY)) return -1;
    return dev_table[f->dev].read(f, buf, len);
}

int bdos_vfs_write(int fd, const void *buf, int len)
{
    bdos_fd_t *f = get_fd(fd);
    if (!f) return -1;
    if (!(f->flags & BDOS_O_WRONLY)) return -1;
    return dev_table[f->dev].write(f, buf, len);
}

int bdos_vfs_lseek(int fd, int offset, int whence)
{
    bdos_fd_t *f = get_fd(fd);
    if (!f) return -1;
    return dev_table[f->dev].lseek(f, offset, whence);
}

/*
 * dup2(oldfd, newfd): make newfd refer to the same underlying object
 * as oldfd. If newfd is currently open it is closed first.
 *
 * Implementation note: there is no refcount on the underlying BRFS
 * handle, so we use *move* semantics rather than *duplicate*: the
 * source slot is vacated (in_use = 0) so a subsequent close(oldfd)
 * is a no-op and the BRFS handle stays alive in newfd. The shell's
 * pipeline executor relies on this when wiring redirects with the
 * `dup2(fd, 1); close(fd);` idiom.
 */
int bdos_vfs_dup2(int oldfd, int newfd)
{
    bdos_fd_t *src = get_fd(oldfd);
    bdos_fd_t *t;

    if (!src) return -1;
    if (oldfd == newfd) return newfd;
    if (newfd < 0 || newfd >= BDOS_FD_MAX) return -1;

    t = fds();
    if (t[newfd].in_use) bdos_vfs_close(newfd);

    t[newfd] = *src;
    t[oldfd].in_use = 0;
    return newfd;
}
