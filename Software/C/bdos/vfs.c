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

#include "bdos.h"
#include "bdos_vfs.h"

#ifndef NULL
#define NULL ((void *)0)
#endif

/* ============================================================ State ===== */

static bdos_fd_t g_fds[BDOS_FD_MAX];

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

typedef struct {
    int (*read )(bdos_fd_t *, void *, int);
    int (*write)(bdos_fd_t *, const void *, int);
    int (*close)(bdos_fd_t *);
    int (*lseek)(bdos_fd_t *, int, int);
} dev_ops_t;

static const dev_ops_t dev_table[] = {
    { NULL,      NULL,       NULL,       NULL       }, /* DEV_NONE — sentinel */
    { tty_read,  tty_write,  tty_close,  tty_lseek  }, /* DEV_TTY  */
    { file_read, file_write, file_close, file_lseek }, /* DEV_FILE */
    { null_read, null_write, null_close, null_lseek }, /* DEV_NULL */
};
#define DEV_TABLE_LEN ((int)(sizeof(dev_table) / sizeof(dev_table[0])))

/* ============================================================ Helpers ==== */

static int alloc_fd(void)
{
    int i;
    for (i = 0; i < BDOS_FD_MAX; i++) {
        if (!g_fds[i].in_use)
            return i;
    }
    return -1;
}

static bdos_fd_t *get_fd(int fd)
{
    if (fd < 0 || fd >= BDOS_FD_MAX) return NULL;
    if (!g_fds[fd].in_use)            return NULL;
    return &g_fds[fd];
}

static int str_eq(const char *a, const char *b)
{
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

/* ============================================================ TTY dev === */

/*
 * Reads ASCII bytes from the keyboard FIFO. Special keys (>= 0x100)
 * are skipped silently — programs that need them must use the raw
 * libterm v2 input API instead. Blocks until at least one byte arrives
 * or EOF (Ctrl-D pressed via libterm's line discipline).
 *
 * For now this is a pure pass-through: each call drains pending events.
 * Phase A.2's libterm cooked-mode line discipline is not wired here yet
 * (it would require a per-tty cooked/raw flag stored on the fd) — that
 * comes when shell v2 lands.
 */
static int tty_read(bdos_fd_t *f, void *buf, int len)
{
    char *out = (char *)buf;
    int n = 0;
    int ev;

    (void)f;
    if (len <= 0) return 0;

    /* Block for first byte */
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

/* ============================================================ File dev == */

static unsigned int file_size_bytes(bdos_fd_t *f)
{
    int wsz = brfs_file_size(f->handle);
    if (wsz < 0) return 0;
    return (unsigned int)wsz * 4u;
}

static int file_read(bdos_fd_t *f, void *buf, int len)
{
    unsigned char *out = (unsigned char *)buf;
    unsigned int   pos = f->byte_pos;
    int            done = 0;
    unsigned int   word_idx, byte_off;
    int            words_need, wr;
    /* Static scratch — fine for single-threaded BDOS. */
    static unsigned int wbuf[64];

    if (len <= 0) return 0;

    while (done < len) {
        word_idx = (pos + (unsigned int)done) / 4u;
        byte_off = (pos + (unsigned int)done) % 4u;

        words_need = (len - done + (int)byte_off + 3) / 4;
        if (words_need > 64) words_need = 64;

        if (brfs_seek(f->handle, word_idx) < 0) break;
        wr = brfs_read(f->handle, wbuf, (unsigned int)words_need);
        if (wr <= 0) break;

        {
            int wi = 0;
            int bo = (int)byte_off;
            unsigned int w;
            while (wi < wr && done < len) {
                w = wbuf[wi];
                while (bo < 4 && done < len) {
                    out[done++] = (unsigned char)((w >> (24 - bo * 8)) & 0xFF);
                    bo++;
                }
                bo = 0;
                wi++;
            }
        }
    }

    f->byte_pos = pos + (unsigned int)done;
    return done;
}

static int file_flush_partial(bdos_fd_t *f)
{
    unsigned int word_idx;
    unsigned int wbuf[1];

    if (f->wr_fill == 0) return 0;

    word_idx = f->byte_pos / 4u;
    /* Partial trailing word: pad with zeros. The byte_pos already
       points past what we've buffered; align to word_idx + 1 boundary. */
    wbuf[0] = f->wr_buf << (8u * (4u - f->wr_fill));
    if (brfs_seek(f->handle, word_idx) < 0) return -1;
    if (brfs_write(f->handle, wbuf, 1u) < 0) return -1;
    f->wr_buf  = 0;
    f->wr_fill = 0;
    return 0;
}

static int file_write(bdos_fd_t *f, const void *buf, int len)
{
    const unsigned char *p = (const unsigned char *)buf;
    int                  done = 0;
    unsigned int         word_idx;
    unsigned int         wbuf[1];

    if (len <= 0) return 0;

    /* If there's an existing wr_fill, top it up to a full word and commit. */
    while (done < len && f->wr_fill > 0 && f->wr_fill < 4) {
        f->wr_buf = (f->wr_buf << 8) | p[done++];
        f->wr_fill++;
    }
    if (f->wr_fill == 4) {
        word_idx = f->byte_pos / 4u;
        wbuf[0]  = f->wr_buf;
        if (brfs_seek(f->handle, word_idx) < 0) return done == 0 ? -1 : done;
        if (brfs_write(f->handle, wbuf, 1u) < 0) return done == 0 ? -1 : done;
        f->byte_pos += 4u;
        f->wr_buf = 0;
        f->wr_fill = 0;
    }

    /* If we're not on a word boundary (mid-file write) read the existing
     * head word so partial-byte writes don't corrupt neighbouring bytes. */
    if (done < len && (f->byte_pos & 3u) != 0u) {
        unsigned int rbuf[1];
        unsigned int bo = f->byte_pos & 3u;
        word_idx = f->byte_pos / 4u;
        if (brfs_seek(f->handle, word_idx) < 0) return done == 0 ? -1 : done;
        if (brfs_read(f->handle, rbuf, 1u) <= 0) rbuf[0] = 0;
        f->wr_buf  = rbuf[0] >> (8u * (4u - bo));
        f->wr_fill = bo;
        while (done < len && f->wr_fill < 4) {
            f->wr_buf = (f->wr_buf << 8) | p[done++];
            f->wr_fill++;
        }
        if (f->wr_fill == 4) {
            wbuf[0] = f->wr_buf;
            if (brfs_seek(f->handle, word_idx) < 0) return done;
            if (brfs_write(f->handle, wbuf, 1u) < 0) return done;
            f->byte_pos = (word_idx + 1u) * 4u;
            f->wr_buf = 0;
            f->wr_fill = 0;
        }
    }

    /* Bulk-commit complete words from the input. */
    while (len - done >= 4) {
        wbuf[0] = ((unsigned int)p[done    ] << 24)
                | ((unsigned int)p[done + 1] << 16)
                | ((unsigned int)p[done + 2] << 8)
                |  (unsigned int)p[done + 3];
        word_idx = f->byte_pos / 4u;
        if (brfs_seek(f->handle, word_idx) < 0) return done;
        if (brfs_write(f->handle, wbuf, 1u) < 0) return done;
        f->byte_pos += 4u;
        done += 4;
    }

    /* Stash the remaining 0..3 trailing bytes. */
    while (done < len) {
        f->wr_buf = (f->wr_buf << 8) | p[done++];
        f->wr_fill++;
    }
    return done;
}

static int file_close(bdos_fd_t *f)
{
    file_flush_partial(f);
    return brfs_close(f->handle);
}

static int file_lseek(bdos_fd_t *f, int off, int whence)
{
    int newpos;

    /* Flush any partial word before moving — otherwise we'd drop bytes. */
    file_flush_partial(f);

    switch (whence) {
        case BDOS_SEEK_SET: newpos = off;                        break;
        case BDOS_SEEK_CUR: newpos = (int)f->byte_pos + off;     break;
        case BDOS_SEEK_END: newpos = (int)file_size_bytes(f) + off; break;
        default: return -1;
    }
    if (newpos < 0) newpos = 0;
    f->byte_pos = (unsigned int)newpos;
    return newpos;
}

/* ============================================================ Null dev == */

static int null_read (bdos_fd_t *f, void *buf, int len)
{ (void)f; (void)buf; (void)len; return 0; }

static int null_write(bdos_fd_t *f, const void *buf, int len)
{ (void)f; (void)buf; return len; }

static int null_close(bdos_fd_t *f) { (void)f; return 0; }
static int null_lseek(bdos_fd_t *f, int off, int whence)
{ (void)f; (void)off; (void)whence; return 0; }

/* ============================================================ Public ==== */

void bdos_vfs_init(void)
{
    int i;
    for (i = 0; i < BDOS_FD_MAX; i++) {
        g_fds[i].in_use = 0;
    }

    /* Pre-open stdin (0), stdout (1), stderr (2) → /dev/tty. */
    for (i = 0; i < 3; i++) {
        g_fds[i].in_use   = 1;
        g_fds[i].dev      = BDOS_DEV_TTY;
        g_fds[i].flags    = (i == 0) ? BDOS_O_RDONLY : BDOS_O_WRONLY;
        g_fds[i].handle   = 0;
        g_fds[i].byte_pos = 0;
        g_fds[i].wr_buf   = 0;
        g_fds[i].wr_fill  = 0;
    }
}

void bdos_vfs_shutdown(void)
{
    int i;
    for (i = 3; i < BDOS_FD_MAX; i++) {
        if (g_fds[i].in_use)
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
    } else {
        dev = BDOS_DEV_FILE;
        handle = brfs_open(path);
        if (handle < 0 && (flags & BDOS_O_CREAT)) {
            if (brfs_create_file(path) < 0) return -1;
            handle = brfs_open(path);
        }
        if (handle < 0) return -1;
    }

    fd = alloc_fd();
    if (fd < 0) {
        if (dev == BDOS_DEV_FILE) brfs_close(handle);
        return -1;
    }
    f = &g_fds[fd];
    f->in_use   = 1;
    f->dev      = dev;
    f->flags    = flags;
    f->handle   = handle;
    f->byte_pos = 0;
    f->wr_buf   = 0;
    f->wr_fill  = 0;
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
