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

/* flags values from libc stdio.c */
#define STDIO_SWR 0x02

int _open(const char *path, int flags)
{
    int fd;

    /* For write mode, create the file if it doesn't exist */
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
     * libc uses negative fds: stdout=-1, stderr=-2.
     * Positive fds (1, 2, ...) are valid BRFS file descriptors. */
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
            sys_putstr(tmp);
            sys_uart_print_str(tmp);
            offset += chunk;
        }
        return len;
    }

    /* File write — pack bytes into words for BRFS.
     * BRFS stores 4 bytes per word (big-endian packed).
     * Handles non-aligned writes via read-modify-write. */
    if (fd < 0 || fd >= MAX_OPEN_FDS || len <= 0)
        return -1;

    int pos = fd_byte_pos[fd];
    int bytes_done = 0;

    while (bytes_done < len) {
        int cur_pos = pos + bytes_done;
        int word_idx = cur_pos / 4;
        int byte_off = cur_pos % 4;

        /* How many bytes fit in this word starting from byte_off */
        int avail = 4 - byte_off;
        int remaining = len - bytes_done;
        int chunk = (remaining < avail) ? remaining : avail;

        unsigned int w;

        if (byte_off != 0 || chunk < 4) {
            /* Partial word: read existing word first */
            unsigned int rbuf[1];
            sys_fs_seek(fd, word_idx);
            int rr = sys_fs_read(fd, rbuf, 1);
            w = (rr > 0) ? rbuf[0] : 0;
        } else {
            w = 0;
        }

        /* Merge new bytes into the word (big-endian packing) */
        int i;
        for (i = 0; i < chunk; i++) {
            int shift = (24 - (byte_off + i) * 8);
            unsigned int mask = 0xFF << shift;
            w = (w & ~mask) | (((unsigned int)(unsigned char)buf[bytes_done + i]) << shift);
        }

        /* Write the word back */
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

/* ---- Stubs for POSIX functions used by Doom ---- */

void mkdir(const char *path, int mode)
{
    sys_fs_mkdir((char *)path);
}

int _remove(const char *pathname)
{
    return sys_fs_delete((char *)pathname);
}

/*
 * _rename — rename a file by copy + delete.
 * BRFS has no native rename, so we read the old file, create the new
 * file, write the data, then delete the old file.
 */
int _rename(const char *oldpath, const char *newpath)
{
    int old_fd;
    int new_fd;
    int file_words;
    unsigned int buf[128];
    int words_left;

    /* Open old file and get its size */
    old_fd = sys_fs_open((char *)oldpath);
    if (old_fd < 0)
        return -1;
    file_words = sys_fs_filesize(old_fd);
    if (file_words < 0) {
        sys_fs_close(old_fd);
        return -1;
    }

    /* Delete destination if it exists, then create it */
    sys_fs_delete((char *)newpath);
    sys_fs_create((char *)newpath);
    new_fd = sys_fs_open((char *)newpath);
    if (new_fd < 0) {
        sys_fs_close(old_fd);
        return -1;
    }

    /* Copy data in 128-word chunks */
    sys_fs_seek(old_fd, 0);
    sys_fs_seek(new_fd, 0);
    words_left = file_words;
    while (words_left > 0) {
        int chunk = (words_left > 128) ? 128 : words_left;
        int rd = sys_fs_read(old_fd, buf, chunk);
        if (rd <= 0)
            break;
        sys_fs_write(new_fd, buf, rd);
        words_left -= rd;
    }

    sys_fs_close(old_fd);
    sys_fs_close(new_fd);

    /* Delete the old file */
    sys_fs_delete((char *)oldpath);
    return 0;
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
