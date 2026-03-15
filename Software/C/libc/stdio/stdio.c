/*
 * stdio implementation for B32P3/FPGC libc.
 *
 * Provides printf family (vsnprintf core), basic FILE operations,
 * and stdout/stderr/stdin streams.
 *
 * The printf core handles: %d, %i, %u, %x, %X, %o, %c, %s, %p, %%, %ld, %lu, %lx
 * with width, precision, zero-padding, left-align, '+' and ' ' flags.
 * No floating-point (B32P3 has no FPU).
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>

/* errno support */
int errno;

/*========================================================================
 * Output callback abstraction
 *======================================================================*/

struct printf_state {
    char *buf;       /* NULL for stream output */
    size_t pos;      /* current position */
    size_t limit;    /* max chars (for snprintf) */
    FILE *stream;    /* output stream (for fprintf) */
    int error;       /* error flag */
};

static void
pf_putchar(struct printf_state *st, char c)
{
    if (st->buf) {
        /* String output (sprintf/snprintf) */
        if (st->pos < st->limit)
            st->buf[st->pos] = c;
        st->pos++;
    } else if (st->stream) {
        /* Stream output */
        if (fputc(c, st->stream) == EOF)
            st->error = 1;
        st->pos++;
    }
}

static void
pf_puts(struct printf_state *st, const char *s, size_t len)
{
    size_t i;
    for (i = 0; i < len; i++)
        pf_putchar(st, s[i]);
}

static void
pf_pad(struct printf_state *st, char c, int count)
{
    while (count-- > 0)
        pf_putchar(st, c);
}

/*========================================================================
 * Integer to string conversion
 *======================================================================*/

static int
utoa_buf(char *buf, unsigned long val, int base, int uppercase)
{
    const char *digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    int len = 0;
    char tmp[12]; /* enough for 32-bit in base 8 */

    if (val == 0) {
        buf[0] = '0';
        return 1;
    }
    while (val > 0) {
        tmp[len++] = digits[val % base];
        val /= base;
    }
    /* Reverse */
    for (int i = 0; i < len; i++)
        buf[i] = tmp[len - 1 - i];
    return len;
}

/*========================================================================
 * vsnprintf core — the heart of all printf variants
 *======================================================================*/

static int
pf_format(struct printf_state *st, const char *fmt, va_list ap)
{
    while (*fmt) {
        if (*fmt != '%') {
            pf_putchar(st, *fmt++);
            continue;
        }
        fmt++; /* skip '%' */

        /* Parse flags */
        int flag_minus = 0;
        int flag_plus = 0;
        int flag_space = 0;
        int flag_zero = 0;
        int flag_hash = 0;
        for (;;) {
            if (*fmt == '-')      { flag_minus = 1; fmt++; }
            else if (*fmt == '+') { flag_plus = 1; fmt++; }
            else if (*fmt == ' ') { flag_space = 1; fmt++; }
            else if (*fmt == '0') { flag_zero = 1; fmt++; }
            else if (*fmt == '#') { flag_hash = 1; fmt++; }
            else break;
        }

        /* Parse width */
        int width = 0;
        if (*fmt == '*') {
            width = va_arg(ap, int);
            if (width < 0) {
                flag_minus = 1;
                width = -width;
            }
            fmt++;
        } else {
            while (*fmt >= '0' && *fmt <= '9') {
                width = width * 10 + (*fmt - '0');
                fmt++;
            }
        }

        /* Parse precision */
        int precision = -1;
        if (*fmt == '.') {
            fmt++;
            precision = 0;
            if (*fmt == '*') {
                precision = va_arg(ap, int);
                if (precision < 0)
                    precision = -1;
                fmt++;
            } else {
                while (*fmt >= '0' && *fmt <= '9') {
                    precision = precision * 10 + (*fmt - '0');
                    fmt++;
                }
            }
        }

        /* Parse length modifier */
        int is_long = 0;
        if (*fmt == 'l') {
            is_long = 1;
            fmt++;
            if (*fmt == 'l') {
                /* ll — treat as long on ILP32 */
                fmt++;
            }
        } else if (*fmt == 'h') {
            fmt++;
            if (*fmt == 'h')
                fmt++; /* hh */
        } else if (*fmt == 'z' || *fmt == 't') {
            is_long = 0; /* size_t/ptrdiff_t == int on ILP32 */
            fmt++;
        }

        /* Conversion specifier */
        char conv = *fmt++;
        if (conv == '\0')
            break;

        char num_buf[12];
        int num_len;
        char sign_char = 0;
        const char *str;
        int str_len;
        int pad_len;
        char pad_char;

        (void)flag_hash;

        switch (conv) {
        case '%':
            pf_putchar(st, '%');
            break;

        case 'c':
            {
                char ch = (char)va_arg(ap, int);
                pad_len = width > 1 ? width - 1 : 0;
                if (!flag_minus)
                    pf_pad(st, ' ', pad_len);
                pf_putchar(st, ch);
                if (flag_minus)
                    pf_pad(st, ' ', pad_len);
            }
            break;

        case 's':
            str = va_arg(ap, const char *);
            if (!str)
                str = "(null)";
            str_len = (int)strlen(str);
            if (precision >= 0 && str_len > precision)
                str_len = precision;
            pad_len = width > str_len ? width - str_len : 0;
            if (!flag_minus)
                pf_pad(st, ' ', pad_len);
            pf_puts(st, str, (size_t)str_len);
            if (flag_minus)
                pf_pad(st, ' ', pad_len);
            break;

        case 'd':
        case 'i':
            {
                long val = is_long ? va_arg(ap, long) : (long)va_arg(ap, int);
                unsigned long uval;
                if (val < 0) {
                    sign_char = '-';
                    uval = (unsigned long)(-val);
                } else {
                    if (flag_plus) sign_char = '+';
                    else if (flag_space) sign_char = ' ';
                    uval = (unsigned long)val;
                }
                num_len = utoa_buf(num_buf, uval, 10, 0);

                /* Apply precision (minimum digits) */
                int total = num_len;
                int zero_pad = 0;
                if (precision >= 0 && precision > num_len) {
                    zero_pad = precision - num_len;
                    total = precision;
                }
                if (sign_char)
                    total++;

                pad_len = width > total ? width - total : 0;
                pad_char = (flag_zero && !flag_minus && precision < 0) ? '0' : ' ';

                if (!flag_minus && pad_char == ' ')
                    pf_pad(st, ' ', pad_len);
                if (sign_char)
                    pf_putchar(st, sign_char);
                if (!flag_minus && pad_char == '0')
                    pf_pad(st, '0', pad_len);
                pf_pad(st, '0', zero_pad);
                pf_puts(st, num_buf, (size_t)num_len);
                if (flag_minus)
                    pf_pad(st, ' ', pad_len);
            }
            break;

        case 'u':
        case 'x':
        case 'X':
        case 'o':
            {
                unsigned long uval = is_long ? va_arg(ap, unsigned long)
                                             : (unsigned long)va_arg(ap, unsigned int);
                int base;
                int uppercase = 0;
                if (conv == 'o')      base = 8;
                else if (conv == 'x') base = 16;
                else if (conv == 'X') { base = 16; uppercase = 1; }
                else                  base = 10;

                num_len = utoa_buf(num_buf, uval, base, uppercase);

                int total = num_len;
                int zero_pad = 0;
                if (precision >= 0 && precision > num_len) {
                    zero_pad = precision - num_len;
                    total = precision;
                }

                pad_len = width > total ? width - total : 0;
                pad_char = (flag_zero && !flag_minus && precision < 0) ? '0' : ' ';

                if (!flag_minus && pad_char == ' ')
                    pf_pad(st, ' ', pad_len);
                if (!flag_minus && pad_char == '0')
                    pf_pad(st, '0', pad_len);
                pf_pad(st, '0', zero_pad);
                pf_puts(st, num_buf, (size_t)num_len);
                if (flag_minus)
                    pf_pad(st, ' ', pad_len);
            }
            break;

        case 'p':
            {
                uintptr_t pval = (uintptr_t)va_arg(ap, void *);
                pf_puts(st, "0x", 2);
                num_len = utoa_buf(num_buf, (unsigned long)pval, 16, 0);
                /* Pad pointer to 8 hex digits */
                pf_pad(st, '0', 8 - num_len);
                pf_puts(st, num_buf, (size_t)num_len);
            }
            break;

        case 'n':
            {
                int *np = va_arg(ap, int *);
                if (np)
                    *np = (int)st->pos;
            }
            break;

        default:
            /* Unknown conversion — output literally */
            pf_putchar(st, '%');
            pf_putchar(st, conv);
            break;
        }
    }
    return (int)st->pos;
}

/*========================================================================
 * Public printf family
 *======================================================================*/

int
vsnprintf(char *str, size_t size, const char *fmt, va_list ap)
{
    struct printf_state st;
    int ret;

    st.buf = str;
    st.pos = 0;
    st.limit = size > 0 ? size - 1 : 0;
    st.stream = NULL;
    st.error = 0;

    ret = pf_format(&st, fmt, ap);

    /* Null-terminate */
    if (str && size > 0)
        str[st.pos < st.limit ? st.pos : st.limit] = '\0';

    return ret;
}

int
vsprintf(char *str, const char *fmt, va_list ap)
{
    return vsnprintf(str, (size_t)INT_MAX, fmt, ap);
}

int
snprintf(char *str, size_t size, const char *fmt, ...)
{
    va_list ap;
    int ret;
    va_start(ap, fmt);
    ret = vsnprintf(str, size, fmt, ap);
    va_end(ap);
    return ret;
}

int
sprintf(char *str, const char *fmt, ...)
{
    va_list ap;
    int ret;
    va_start(ap, fmt);
    ret = vsprintf(str, fmt, ap);
    va_end(ap);
    return ret;
}

/*========================================================================
 * FILE I/O — minimal implementation
 *======================================================================*/

/* Internal FILE structure */
struct __stdio_file {
    int fd;          /* underlying file descriptor (-1=stdout, -2=stderr, -3=stdin) */
    int flags;       /* SRD, SWR, SERR, SEOF */
    int ungetc_buf;  /* ungetc buffer (-1 = empty) */
};

#define STDIO_FD_STDIN  (-3)
#define STDIO_FD_STDOUT (-1)
#define STDIO_FD_STDERR (-2)

#define STDIO_SRD  0x01
#define STDIO_SWR  0x02
#define STDIO_SERR 0x04
#define STDIO_SEOF 0x08

static struct __stdio_file __stdin_file  = { STDIO_FD_STDIN,  STDIO_SRD, -1 };
static struct __stdio_file __stdout_file = { STDIO_FD_STDOUT, STDIO_SWR, -1 };
static struct __stdio_file __stderr_file = { STDIO_FD_STDERR, STDIO_SWR, -1 };

FILE *stdin  = &__stdin_file;
FILE *stdout = &__stdout_file;
FILE *stderr = &__stderr_file;

/* Platform-provided low-level I/O */
extern int _write(int fd, const char *buf, int len);
extern int _read(int fd, char *buf, int len);
extern int _open(const char *path, int flags);
extern int _close(int fd);
extern int _lseek(int fd, int offset, int whence);

/*------------------------------------------------------------------------
 * fputc / putchar
 *----------------------------------------------------------------------*/
int
fputc(int c, FILE *stream)
{
    char ch = (char)c;
    if (_write(stream->fd, &ch, 1) != 1) {
        stream->flags |= STDIO_SERR;
        return EOF;
    }
    return (unsigned char)c;
}

int
putchar(int c)
{
    return fputc(c, stdout);
}

/*------------------------------------------------------------------------
 * fputs / puts
 *----------------------------------------------------------------------*/
int
fputs(const char *s, FILE *stream)
{
    int len = (int)strlen(s);
    if (_write(stream->fd, s, len) != len) {
        stream->flags |= STDIO_SERR;
        return EOF;
    }
    return 0;
}

int
puts(const char *s)
{
    if (fputs(s, stdout) == EOF)
        return EOF;
    return fputc('\n', stdout);
}

/*------------------------------------------------------------------------
 * fgetc / getchar / ungetc
 *----------------------------------------------------------------------*/
int
fgetc(FILE *stream)
{
    char c;
    if (stream->ungetc_buf >= 0) {
        int ch = stream->ungetc_buf;
        stream->ungetc_buf = -1;
        return ch;
    }
    if (_read(stream->fd, &c, 1) != 1) {
        stream->flags |= STDIO_SEOF;
        return EOF;
    }
    return (unsigned char)c;
}

int
getchar(void)
{
    return fgetc(stdin);
}

int
ungetc(int c, FILE *stream)
{
    if (c == EOF)
        return EOF;
    stream->ungetc_buf = c;
    stream->flags &= ~STDIO_SEOF;
    return c;
}

/*------------------------------------------------------------------------
 * fread / fwrite
 *----------------------------------------------------------------------*/
size_t
fread(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    size_t total = size * nmemb;
    int n;
    if (total == 0)
        return 0;
    n = _read(stream->fd, (char *)ptr, (int)total);
    if (n <= 0) {
        stream->flags |= STDIO_SEOF;
        return 0;
    }
    return (size_t)n / size;
}

size_t
fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    size_t total = size * nmemb;
    int n;
    if (total == 0)
        return 0;
    n = _write(stream->fd, (const char *)ptr, (int)total);
    if (n <= 0) {
        stream->flags |= STDIO_SERR;
        return 0;
    }
    return (size_t)n / size;
}

/*------------------------------------------------------------------------
 * File open / close / seek / tell
 *----------------------------------------------------------------------*/

/* Maximum open file handles (excluding stdin/stdout/stderr) */
#define STDIO_MAX_FILES 16
static struct __stdio_file file_pool[STDIO_MAX_FILES];
static int file_pool_used[STDIO_MAX_FILES];

FILE *
fopen(const char *path, const char *mode)
{
    int flags = 0;
    int fd;
    int i;

    if (mode[0] == 'r')      flags = STDIO_SRD;
    else if (mode[0] == 'w') flags = STDIO_SWR;
    else if (mode[0] == 'a') flags = STDIO_SWR;
    else return NULL;

    fd = _open(path, flags);
    if (fd < 0)
        return NULL;

    /* Find a free pool slot */
    for (i = 0; i < STDIO_MAX_FILES; i++) {
        if (!file_pool_used[i]) {
            file_pool_used[i] = 1;
            file_pool[i].fd = fd;
            file_pool[i].flags = flags;
            file_pool[i].ungetc_buf = -1;
            return &file_pool[i];
        }
    }
    _close(fd);
    return NULL;
}

int
fclose(FILE *stream)
{
    int i;
    int ret;

    if (!stream)
        return EOF;

    ret = _close(stream->fd);

    /* Return slot to pool */
    for (i = 0; i < STDIO_MAX_FILES; i++) {
        if (&file_pool[i] == stream) {
            file_pool_used[i] = 0;
            break;
        }
    }
    return ret;
}

int
fflush(FILE *stream)
{
    (void)stream;
    return 0;  /* unbuffered */
}

int
fseek(FILE *stream, long offset, int whence)
{
    return _lseek(stream->fd, (int)offset, whence);
}

long
ftell(FILE *stream)
{
    return (long)_lseek(stream->fd, 0, SEEK_CUR);
}

void
rewind(FILE *stream)
{
    fseek(stream, 0, SEEK_SET);
    stream->flags &= ~(STDIO_SERR | STDIO_SEOF);
}

int
feof(FILE *stream)
{
    return (stream->flags & STDIO_SEOF) != 0;
}

int
ferror(FILE *stream)
{
    return (stream->flags & STDIO_SERR) != 0;
}

void
clearerr(FILE *stream)
{
    stream->flags &= ~(STDIO_SERR | STDIO_SEOF);
}

/*------------------------------------------------------------------------
 * printf / fprintf / vprintf / vfprintf
 *----------------------------------------------------------------------*/
int
vfprintf(FILE *stream, const char *fmt, va_list ap)
{
    struct printf_state st;
    st.buf = NULL;
    st.pos = 0;
    st.limit = 0;
    st.stream = stream;
    st.error = 0;
    return pf_format(&st, fmt, ap);
}

int
vprintf(const char *fmt, va_list ap)
{
    return vfprintf(stdout, fmt, ap);
}

int
printf(const char *fmt, ...)
{
    va_list ap;
    int ret;
    va_start(ap, fmt);
    ret = vprintf(fmt, ap);
    va_end(ap);
    return ret;
}

int
fprintf(FILE *stream, const char *fmt, ...)
{
    va_list ap;
    int ret;
    va_start(ap, fmt);
    ret = vfprintf(stream, fmt, ap);
    va_end(ap);
    return ret;
}

/*------------------------------------------------------------------------
 * sscanf — minimal (not implemented, stub)
 *----------------------------------------------------------------------*/
int
sscanf(const char *str, const char *fmt, ...)
{
    (void)str;
    (void)fmt;
    return 0;
}

/*------------------------------------------------------------------------
 * remove — stub
 *----------------------------------------------------------------------*/
int
remove(const char *pathname)
{
    (void)pathname;
    return -1;
}

/*------------------------------------------------------------------------
 * exit / abort — program termination
 *----------------------------------------------------------------------*/
extern void _exit(int code);

void
exit(int status)
{
    _exit(status);
}

void
abort(void)
{
    _exit(1);
}

/*------------------------------------------------------------------------
 * __assert_fail — assertion failure handler
 *----------------------------------------------------------------------*/
void
__assert_fail(const char *expr, const char *file, int line)
{
    printf("Assertion failed: %s at %s:%d\n", expr, file, line);
    abort();
}
