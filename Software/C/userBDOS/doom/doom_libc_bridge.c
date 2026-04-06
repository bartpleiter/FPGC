/*
 * doom_libc_bridge.c — Bridge libc FILE I/O to BDOS syscalls.
 *
 * BRFS is word-oriented: all reads, writes, seeks, and sizes are in
 * 32-bit words (4 bytes per word, big-endian packed).  The standard C
 * library expects byte-oriented I/O.  This bridge translates between
 * the two by:
 *   - tracking a byte-level cursor per open fd
 *   - reading words from BRFS and unpacking bytes
 *   - packing bytes into words for BRFS writes
 *   - converting byte offsets to word offsets for seeking
 */

#include <syscall.h>
#include <stddef.h>

/* ---- Per-fd byte cursor tracking ---- */
#define MAX_OPEN_FDS 16
static int  fd_byte_pos[MAX_OPEN_FDS];    /* byte cursor       */
static int  fd_open[MAX_OPEN_FDS];        /* 1 if slot in use  */

/* ---- File I/O bridge ---- */

int _open(const char *path, int flags)
{
    int fd = sys_fs_open((char *)path);
    sys_uart_print_str("_open(\"");
    sys_uart_print_str((char *)path);
    sys_uart_print_str("\") = ");
    if (fd < 0) {
        sys_uart_print_str("-1");
    } else {
        char buf[12];
        int i = 11;
        int n = fd;
        buf[i] = 0;
        if (n == 0) buf[--i] = '0';
        while (n > 0) { buf[--i] = '0' + (n % 10); n /= 10; }
        sys_uart_print_str(&buf[i]);
    }
    sys_uart_print_str("\n");

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

/*
 * _read — read `len` bytes from file `fd` into `buf`.
 *
 * BRFS stores 4 bytes per word (big-endian).  We read whole words and
 * unpack the requested byte range.
 */
int _read(int fd, char *buf, int len)
{
    if (fd < 0 || fd >= MAX_OPEN_FDS || len <= 0)
        return -1;

    int pos = fd_byte_pos[fd];
    int start_word = pos / 4;
    int start_byte = pos % 4;   /* byte offset within first word */
    int bytes_done = 0;

    /* Seek BRFS to the first word we need */
    sys_fs_seek(fd, start_word);

    /* Chunk buffer — read up to 128 words (512 bytes) at a time */
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

        /* Unpack bytes from the words we just read */
        int wi = 0;
        int bo = byte_off;            /* byte within current word */
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

/*
 * _lseek — seek to a byte position in the file.
 *
 * BRFS seek/tell/filesize are all in WORDS.  We convert.
 */
int _lseek(int fd, int offset, int whence)
{
    if (fd < 0 || fd >= MAX_OPEN_FDS)
        return -1;

    int new_byte_pos;

    if (whence == 0) {
        /* SEEK_SET */
        new_byte_pos = offset;
    } else if (whence == 1) {
        /* SEEK_CUR */
        new_byte_pos = fd_byte_pos[fd] + offset;
    } else if (whence == 2) {
        /* SEEK_END — filesize is in words, convert to bytes */
        int word_size = sys_fs_filesize(fd);
        if (word_size < 0) return -1;
        new_byte_pos = word_size * 4 + offset;
    } else {
        return -1;
    }

    if (new_byte_pos < 0)
        new_byte_pos = 0;

    fd_byte_pos[fd] = new_byte_pos;

    /* Also position the BRFS cursor at the right word so a subsequent
     * brfs_read starts from the correct block. */
    sys_fs_seek(fd, new_byte_pos / 4);

    return new_byte_pos;
}

int _write(int fd, const char *buf, int len)
{
    /* stdout/stderr → BDOS terminal + UART.
     * Output to the BDOS screen terminal so startup log is visible,
     * and also to UART for serial debugging. */
    if (fd == 1 || fd == 2 || fd == -1 || fd == -2) {
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
    int start_word = pos / 4;
    int start_byte = pos % 4;

    /* For simplicity, only support word-aligned writes for now.
     * Doom primarily writes config files which are text. */
    if (start_byte != 0) {
        /* Non-aligned write — not yet supported */
        return -1;
    }

    int word_count = (len + 3) / 4;
    unsigned int wbuf[128];
    int bytes_done = 0;

    sys_fs_seek(fd, start_word);

    while (bytes_done < len) {
        int remaining = len - bytes_done;
        int wc = (remaining + 3) / 4;
        if (wc > 128) wc = 128;

        /* Pack bytes into words (big-endian, pad with 0) */
        int wi;
        for (wi = 0; wi < wc; wi++) {
            unsigned int w = 0;
            int bi;
            for (bi = 0; bi < 4; bi++) {
                int idx = bytes_done + wi * 4 + bi;
                if (idx < len)
                    w |= ((unsigned int)(unsigned char)buf[idx]) << (24 - bi * 8);
            }
            wbuf[wi] = w;
        }

        int wr = sys_fs_write(fd, wbuf, wc);
        if (wr <= 0) break;
        bytes_done += wr * 4;
        if (bytes_done > len) bytes_done = len;
    }

    fd_byte_pos[fd] = pos + bytes_done;
    return bytes_done;
}

/* ---- Stubs for POSIX functions used by Doom ---- */

void mkdir(const char *path, int mode)
{
    /* BRFS doesn't have directories — no-op */
}

/* ---- String utilities ---- */

int strcasecmp(const char *s1, const char *s2)
{
    while (*s1 && *s2) {
        int c1 = *s1;
        int c2 = *s2;
        if (c1 >= 'A' && c1 <= 'Z') c1 += 32;
        if (c2 >= 'A' && c2 <= 'Z') c2 += 32;
        if (c1 != c2) return c1 - c2;
        s1++;
        s2++;
    }
    return (unsigned char)*s1 - (unsigned char)*s2;
}

int strncasecmp(const char *s1, const char *s2, unsigned int n)
{
    while (n-- > 0 && *s1 && *s2) {
        int c1 = *s1;
        int c2 = *s2;
        if (c1 >= 'A' && c1 <= 'Z') c1 += 32;
        if (c2 >= 'A' && c2 <= 'Z') c2 += 32;
        if (c1 != c2) return c1 - c2;
        s1++;
        s2++;
    }
    if (n == (unsigned int)-1) return 0;
    return (unsigned char)*s1 - (unsigned char)*s2;
}
