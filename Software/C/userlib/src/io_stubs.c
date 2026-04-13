/*
 * io_stubs.c — libc I/O implementation for userBDOS programs.
 *
 * Provides _write, _read, _open, _close, _lseek implementations
 * required by stdio.c working with BRFS filesystem.
 *
 * BRFS is word-addressed: each word holds 4 bytes (big-endian packed).
 * This layer translates between byte-oriented C I/O and word-oriented BRFS.
 *
 * stdout (fd -1) and stderr (fd -2) route to the BDOS terminal.
 */

#include <syscall.h>

/* BRFS allows small number of open files */
#define MAX_OPEN_FDS 16

static int fd_byte_pos[MAX_OPEN_FDS];  /* per-fd byte cursor */
static int fd_open[MAX_OPEN_FDS];      /* 1 if slot in use */

/*
 * _io_init — close any stale BRFS file descriptors from previous runs.
 * Called from crt0 before main() to ensure a clean state even after crashes.
 */
void _io_init(void)
{
    int i;
    for (i = 0; i < MAX_OPEN_FDS; i++) {
        sys_fs_close(i);
        fd_byte_pos[i] = 0;
        fd_open[i] = 0;
    }
}

/* stdio flags (must match stdio.c) */
#define STDIO_SRD  1
#define STDIO_SWR  2

int _write(int fd, const char *buf, int len)
{
    /* stdout/stderr → BDOS terminal */
    if (fd == 1 || fd == 2 || fd == -1 || fd == -2) {
        int i;
        for (i = 0; i < len; i++)
            sys_print_char(buf[i]);
        return len;
    }

    /* File write — pack bytes into words for BRFS */
    if (fd < 0 || fd >= MAX_OPEN_FDS || len <= 0)
        return -1;

    int pos = fd_byte_pos[fd];
    int bytes_done = 0;

    while (bytes_done < len) {
        int cur_pos = pos + bytes_done;
        int word_idx = cur_pos / 4;
        int byte_off = cur_pos % 4;

        int avail = 4 - byte_off;
        int remaining = len - bytes_done;
        int chunk = (remaining < avail) ? remaining : avail;

        unsigned int w;

        if (byte_off != 0 || chunk < 4) {
            /* Partial word: read-modify-write */
            unsigned int rbuf[1];
            sys_fs_seek(fd, word_idx);
            int rr = sys_fs_read(fd, rbuf, 1);
            w = (rr > 0) ? rbuf[0] : 0;
        } else {
            w = 0;
        }

        /* Merge bytes into word (big-endian) */
        int i;
        for (i = 0; i < chunk; i++) {
            int shift = (24 - (byte_off + i) * 8);
            unsigned int mask = 0xFFu << shift;
            w = (w & ~mask) | (((unsigned int)(unsigned char)buf[bytes_done + i]) << shift);
        }

        unsigned int wbuf[1];
        wbuf[0] = w;
        sys_fs_seek(fd, word_idx);
        int wr = sys_fs_write(fd, wbuf, 1);
        if (wr <= 0)
            break;

        bytes_done += chunk;
    }

    fd_byte_pos[fd] = pos + bytes_done;
    return bytes_done;
}

int _read(int fd, char *buf, int len)
{
    if (fd < 0 || fd >= MAX_OPEN_FDS || len <= 0)
        return -1;

    int pos = fd_byte_pos[fd];
    int start_word = pos / 4;
    int start_byte = pos % 4;
    int bytes_done = 0;

    sys_fs_seek(fd, start_word);

    unsigned int wbuf[128];

    while (bytes_done < len) {
        int remaining   = len - bytes_done;
        int byte_off    = (bytes_done == 0) ? start_byte : 0;
        int words_need  = (remaining + byte_off + 3) / 4;
        if (words_need > 128)
            words_need = 128;

        int wr = sys_fs_read(fd, wbuf, words_need);
        if (wr <= 0)
            break;

        int wi = 0;
        int bo = byte_off;
        while (wi < wr && bytes_done < len) {
            unsigned int w = wbuf[wi];
            while (bo < 4 && bytes_done < len) {
                char c = (char)((w >> (24 - bo * 8)) & 0xFF);
                if (c == 0) {
                    /* NUL byte = end of file content (BRFS convention) */
                    fd_byte_pos[fd] = pos + bytes_done;
                    return bytes_done;
                }
                buf[bytes_done++] = c;
                bo++;
            }
            bo = 0;
            wi++;
        }
    }

    fd_byte_pos[fd] = pos + bytes_done;
    return bytes_done;
}

int _open(const char *path, int flags)
{
    int fd;

    if (flags & STDIO_SWR) {
        /* Write mode: create if doesn't exist */
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
        fd_open[fd] = 1;
    }
    return fd;
}

int _close(int fd)
{
    if (fd >= 0 && fd < MAX_OPEN_FDS) {
        fd_byte_pos[fd] = 0;
        fd_open[fd] = 0;
    }
    return sys_fs_close(fd);
}

int _lseek(int fd, int offset, int whence)
{
    if (fd < 0 || fd >= MAX_OPEN_FDS)
        return -1;

    int new_byte_pos;

    if (whence == 0) {          /* SEEK_SET */
        new_byte_pos = offset;
    } else if (whence == 1) {   /* SEEK_CUR */
        new_byte_pos = fd_byte_pos[fd] + offset;
    } else if (whence == 2) {   /* SEEK_END */
        int word_size = sys_fs_filesize(fd);
        if (word_size < 0) return -1;
        new_byte_pos = word_size * 4 + offset;
    } else {
        return -1;
    }

    if (new_byte_pos < 0)
        new_byte_pos = 0;

    fd_byte_pos[fd] = new_byte_pos;
    sys_fs_seek(fd, new_byte_pos / 4);

    return new_byte_pos;
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
