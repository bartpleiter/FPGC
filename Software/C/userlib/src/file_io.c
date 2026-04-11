/*
 * file_io.c — Generic libc I/O bridge for userBDOS programs with file access.
 *
 * Translates byte-oriented libc I/O (_open, _close, _read, _write, _lseek)
 * to BRFS word-oriented syscalls.  BRFS stores 4 bytes per word (big-endian).
 *
 * Link this instead of io_stubs.c when a program needs real file I/O.
 */

#include <syscall.h>

/* Per-fd byte cursor tracking */
#define MAX_OPEN_FDS 16
static int fd_byte_pos[MAX_OPEN_FDS];
static int fd_open[MAX_OPEN_FDS];

/* flags values from libc stdio.c */
#define STDIO_SWR 0x02

int _open(const char *path, int flags)
{
    int fd;

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
                buf[bytes_done++] = (char)((w >> (24 - bo * 8)) & 0xFF);
                bo++;
            }
            bo = 0;
            wi++;
        }
    }

    fd_byte_pos[fd] = pos + bytes_done;
    return bytes_done;
}

int _lseek(int fd, int offset, int whence)
{
    if (fd < 0 || fd >= MAX_OPEN_FDS)
        return -1;

    int new_byte_pos;

    if (whence == 0) {
        new_byte_pos = offset;
    } else if (whence == 1) {
        new_byte_pos = fd_byte_pos[fd] + offset;
    } else if (whence == 2) {
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

int _write(int fd, const char *buf, int len)
{
    /* stdout/stderr → BDOS terminal */
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
            sys_print_str(tmp);
            offset += chunk;
        }
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
            unsigned int rbuf[1];
            sys_fs_seek(fd, word_idx);
            int rr = sys_fs_read(fd, rbuf, 1);
            w = (rr > 0) ? rbuf[0] : 0;
        } else {
            w = 0;
        }

        int i;
        for (i = 0; i < chunk; i++) {
            int shift = (24 - (byte_off + i) * 8);
            unsigned int mask = 0xFF << shift;
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

int _remove(const char *pathname)
{
    return sys_fs_delete((char *)pathname);
}

int _rename(const char *oldpath, const char *newpath)
{
    return -1; /* BRFS has no native rename */
}
