#include "libs/common/stdio.h"
#include "libs/common/string.h"

/*
 * Standard I/O Library Implementation
 * Minimal implementation for FPGC.
 */

/* Hardware addresses */
#define UART_TX_ADDR    0x7000000   /* UART transmit register */
#define UART_RX_ADDR    0x7000001   /* UART receive register*/

/* Standard stream structures - initialized on first use */
static FILE _stdin_file;
static FILE _stdout_file;
static FILE _stderr_file;
static int _stdio_initialized = 0;

FILE *stdin  = (FILE*)0;  /* Will be set in _init_stdio */
FILE *stdout = (FILE*)0;
FILE *stderr = (FILE*)0;

/* Initialize stdio subsystem */
static void _init_stdio(void)
{
    if (_stdio_initialized)
    {
        return;
    }
    
    _stdin_file.fd = STDIN_FILENO;
    _stdin_file.flags = _STDIO_READ;
    _stdin_file.eof = 0;
    _stdin_file.error = 0;
    
    _stdout_file.fd = STDOUT_FILENO;
    _stdout_file.flags = _STDIO_WRITE;
    _stdout_file.eof = 0;
    _stdout_file.error = 0;
    
    _stderr_file.fd = STDERR_FILENO;
    _stderr_file.flags = _STDIO_WRITE;
    _stderr_file.eof = 0;
    _stderr_file.error = 0;
    
    stdin = &_stdin_file;
    stdout = &_stdout_file;
    stderr = &_stderr_file;
    
    _stdio_initialized = 1;
}

/* Internal output function pointer for printf family */
typedef void (*output_func_t)(char c, void *ctx);

/* UART output */
static void uart_putc(char c)
{
    volatile unsigned int *uart = (volatile unsigned int *)UART_TX_ADDR;
    *uart = (unsigned int)c;
}

/* Character output */

int putchar(int c)
{
    uart_putc((char)c);
    return c;
}

int puts(const char *s)
{
    while (*s)
    {
        putchar(*s++);
    }
    putchar('\n');
    return 0;
}

/* Character input */

int getchar(void)
{
    /*
     * TODO: Implement when UART RX or keyboard input is available.
     * For now, return EOF.
     */
    return EOF;
}

/*
 * Internal printf implementation
 * Supports: %d, %i, %u, %x, %X, %c, %s, %p, %%
 * Also supports field width, precision, and flags: -, +, space, 0, #
 */

/* Format flags */
#define FLAG_LEFT   0x01    /* Left justify */
#define FLAG_PLUS   0x02    /* Show + for positive numbers */
#define FLAG_SPACE  0x04    /* Space before positive numbers */
#define FLAG_HASH   0x08    /* Alternate form (0x prefix) */
#define FLAG_ZERO   0x10    /* Zero pad */

/* Output context for sprintf/snprintf */
typedef struct
{
    char *buf;
    size_t size;
    size_t pos;
    int overflow;
} string_output_ctx_t;

/* Output functions */
static void output_char_stdout(char c, void *ctx)
{
    (void)ctx;
    putchar(c);
}

static void output_char_string(char c, void *ctx)
{
    string_output_ctx_t *sctx = (string_output_ctx_t *)ctx;
    
    if (sctx->pos < sctx->size - 1)
    {
        sctx->buf[sctx->pos] = c;
    }
    else
    {
        sctx->overflow = 1;
    }
    sctx->pos++;
}

/* Convert unsigned integer to string (returns pointer to end of string) */
static char *utoa_internal(unsigned int value, char *buf, int base, int uppercase)
{
    static const char digits_lower[] = "0123456789abcdef";
    static const char digits_upper[] = "0123456789ABCDEF";
    const char *digits = uppercase ? digits_upper : digits_lower;
    char *p = buf;
    char *first = buf;
    char tmp;

    /* Generate digits in reverse order */
    do
    {
        *p++ = digits[value % base];
        value /= base;
    } while (value > 0);

    *p = '\0';

    /* Reverse the string */
    p--;
    while (first < p)
    {
        tmp = *first;
        *first++ = *p;
        *p-- = tmp;
    }

    return buf;
}

/* Convert signed integer to string */
static char *itoa_internal(int value, char *buf, int base)
{
    char *p = buf;

    if (value < 0 && base == 10)
    {
        *p++ = '-';
        value = -value;
    }

    utoa_internal((unsigned int)value, p, base, 0);

    return buf;
}

/* Output padding characters */
static int output_padding(output_func_t out, void *ctx, int count, char pad_char)
{
    int i;
    for (i = 0; i < count; i++)
    {
        out(pad_char, ctx);
    }
    return count;
}

/* Output string with padding */
static int output_string(output_func_t out, void *ctx, const char *str, 
                         int len, int width, int flags)
{
    int written = 0;
    int padding = width - len;

    if (padding < 0)
    {
        padding = 0;
    }

    /* Left padding */
    if (!(flags & FLAG_LEFT) && padding > 0)
    {
        written += output_padding(out, ctx, padding, ' ');
    }

    /* Output string */
    while (len-- > 0)
    {
        out(*str++, ctx);
        written++;
    }

    /* Right padding */
    if ((flags & FLAG_LEFT) && padding > 0)
    {
        written += output_padding(out, ctx, padding, ' ');
    }

    return written;
}

/* Output number with formatting */
static int output_number(output_func_t out, void *ctx, char *num_str, int len,
                         int is_negative, int width, int precision, int flags,
                         char prefix_char, const char *prefix_str)
{
    int written = 0;
    int prefix_len = 0;
    int sign_char = 0;
    int num_padding = 0;
    int total_len;
    char pad_char = (flags & FLAG_ZERO) && !(flags & FLAG_LEFT) ? '0' : ' ';

    /* Determine sign character */
    if (is_negative)
    {
        sign_char = '-';
    }
    else if (flags & FLAG_PLUS)
    {
        sign_char = '+';
    }
    else if (flags & FLAG_SPACE)
    {
        sign_char = ' ';
    }

    /* Calculate prefix length */
    if (prefix_str && (flags & FLAG_HASH))
    {
        prefix_len = strlen(prefix_str);
    }

    /* Calculate padding needed for precision */
    if (precision > len)
    {
        num_padding = precision - len;
    }

    /* Total length */
    total_len = len + num_padding + prefix_len + (sign_char ? 1 : 0);

    /* Left padding with spaces (if not zero-padding) */
    if (!(flags & FLAG_LEFT) && width > total_len && pad_char == ' ')
    {
        written += output_padding(out, ctx, width - total_len, ' ');
    }

    /* Sign character */
    if (sign_char)
    {
        out(sign_char, ctx);
        written++;
    }

    /* Prefix (0x, etc.) */
    if (prefix_len > 0)
    {
        while (*prefix_str)
        {
            out(*prefix_str++, ctx);
            written++;
        }
    }

    /* Zero padding (either from precision or width) */
    if (pad_char == '0' && width > total_len)
    {
        written += output_padding(out, ctx, width - total_len, '0');
    }
    if (num_padding > 0)
    {
        written += output_padding(out, ctx, num_padding, '0');
    }

    /* Number digits */
    while (*num_str)
    {
        out(*num_str++, ctx);
        written++;
    }

    /* Right padding */
    if ((flags & FLAG_LEFT) && width > total_len)
    {
        written += output_padding(out, ctx, width - total_len, ' ');
    }

    return written;
}

/* Main printf formatting function */
static int vprintf_internal(output_func_t out, void *ctx, 
                            const char *format, unsigned int *args)
{
    int written = 0;
    char num_buf[32];
    const char *s;
    int flags, width, precision;
    int arg_idx = 0;
    int len;
    int is_long;

    while (*format)
    {
        if (*format != '%')
        {
            out(*format++, ctx);
            written++;
            continue;
        }

        format++; /* Skip '%' */

        /* Check for %% */
        if (*format == '%')
        {
            out('%', ctx);
            written++;
            format++;
            continue;
        }

        /* Parse flags */
        flags = 0;
        while (1)
        {
            if (*format == '-')
            {
                flags |= FLAG_LEFT;
            }
            else if (*format == '+')
            {
                flags |= FLAG_PLUS;
            }
            else if (*format == ' ')
            {
                flags |= FLAG_SPACE;
            }
            else if (*format == '#')
            {
                flags |= FLAG_HASH;
            }
            else if (*format == '0')
            {
                flags |= FLAG_ZERO;
            }
            else
            {
                break;
            }
            format++;
        }

        /* Parse width */
        width = 0;
        if (*format == '*')
        {
            width = args[arg_idx++];
            format++;
        }
        else
        {
            while (*format >= '0' && *format <= '9')
            {
                width = width * 10 + (*format - '0');
                format++;
            }
        }

        /* Parse precision */
        precision = -1;
        if (*format == '.')
        {
            format++;
            precision = 0;
            if (*format == '*')
            {
                precision = args[arg_idx++];
                format++;
            }
            else
            {
                while (*format >= '0' && *format <= '9')
                {
                    precision = precision * 10 + (*format - '0');
                    format++;
                }
            }
        }

        /* Parse length modifier */
        is_long = 0;
        if (*format == 'l')
        {
            is_long = 1;
            format++;
            if (*format == 'l')
            {
                format++; /* ll - treat as long */
            }
        }
        else if (*format == 'h')
        {
            format++;
            if (*format == 'h')
            {
                format++; /* hh - treat as int */
            }
        }

        /* Process conversion specifier */
        switch (*format)
        {
            case 'd':
            case 'i':
            {
                int val = (int)args[arg_idx++];
                int is_neg = (val < 0);
                if (is_neg)
                {
                    val = -val;
                }
                utoa_internal((unsigned int)val, num_buf, 10, 0);
                len = strlen(num_buf);
                if (precision < 0) precision = 1;
                written += output_number(out, ctx, num_buf, len, is_neg,
                                        width, precision, flags, 0, NULL);
                break;
            }

            case 'u':
            {
                unsigned int val = args[arg_idx++];
                utoa_internal(val, num_buf, 10, 0);
                len = strlen(num_buf);
                if (precision < 0) precision = 1;
                written += output_number(out, ctx, num_buf, len, 0,
                                        width, precision, flags, 0, NULL);
                break;
            }

            case 'x':
            case 'X':
            {
                unsigned int val = args[arg_idx++];
                int uppercase = (*format == 'X');
                utoa_internal(val, num_buf, 16, uppercase);
                len = strlen(num_buf);
                if (precision < 0) precision = 1;
                written += output_number(out, ctx, num_buf, len, 0,
                                        width, precision, flags, 0,
                                        (flags & FLAG_HASH) ? (uppercase ? "0X" : "0x") : NULL);
                break;
            }

            case 'o':
            {
                unsigned int val = args[arg_idx++];
                utoa_internal(val, num_buf, 8, 0);
                len = strlen(num_buf);
                if (precision < 0) precision = 1;
                written += output_number(out, ctx, num_buf, len, 0,
                                        width, precision, flags, 0,
                                        (flags & FLAG_HASH) ? "0" : NULL);
                break;
            }

            case 'c':
            {
                char c = (char)args[arg_idx++];
                if (!(flags & FLAG_LEFT) && width > 1)
                {
                    written += output_padding(out, ctx, width - 1, ' ');
                }
                out(c, ctx);
                written++;
                if ((flags & FLAG_LEFT) && width > 1)
                {
                    written += output_padding(out, ctx, width - 1, ' ');
                }
                break;
            }

            case 's':
            {
                s = (const char *)args[arg_idx++];
                if (s == NULL)
                {
                    s = "(null)";
                }
                len = strlen(s);
                if (precision >= 0 && precision < len)
                {
                    len = precision;
                }
                written += output_string(out, ctx, s, len, width, flags);
                break;
            }

            case 'p':
            {
                unsigned int val = args[arg_idx++];
                flags |= FLAG_HASH;
                utoa_internal(val, num_buf, 16, 0);
                len = strlen(num_buf);
                written += output_number(out, ctx, num_buf, len, 0,
                                        width, 8, flags, 0, "0x");
                break;
            }

            case 'n':
            {
                int *np = (int *)args[arg_idx++];
                if (np)
                {
                    *np = written;
                }
                break;
            }

            default:
                /* Unknown specifier, output as-is */
                out('%', ctx);
                out(*format, ctx);
                written += 2;
                break;
        }

        format++;
    }

    return written;
}

/* Public printf functions */

int printf(const char *format, ...)
{
    unsigned int *args = (unsigned int *)&format + 1;
    return vprintf_internal(output_char_stdout, NULL, format, args);
}

int sprintf(char *str, const char *format, ...)
{
    unsigned int *args = (unsigned int *)&format + 1;
    string_output_ctx_t ctx;
    int result;

    ctx.buf = str;
    ctx.size = 0x7FFFFFFF; /* No limit */
    ctx.pos = 0;
    ctx.overflow = 0;

    result = vprintf_internal(output_char_string, &ctx, format, args);
    str[ctx.pos] = '\0';

    return result;
}

int snprintf(char *str, size_t size, const char *format, ...)
{
    unsigned int *args = (unsigned int *)&format + 1;
    string_output_ctx_t ctx;
    int result;

    if (size == 0)
    {
        return 0;
    }

    ctx.buf = str;
    ctx.size = size;
    ctx.pos = 0;
    ctx.overflow = 0;

    result = vprintf_internal(output_char_string, &ctx, format, args);
    
    /* Null terminate */
    if (ctx.pos < size)
    {
        str[ctx.pos] = '\0';
    }
    else
    {
        str[size - 1] = '\0';
    }

    return result;
}

/* File operations - stubs for future filesystem implementation */

FILE *fopen(const char *pathname, const char *mode)
{
    /*
     * TODO: Implement when filesystem is available.
     * For now, return NULL (file not found).
     */
    (void)pathname;
    (void)mode;
    return NULL;
}

int fclose(FILE *stream)
{
    if (stream == NULL)
    {
        return EOF;
    }

    /* Don't close standard streams */
    if (stream == stdin || stream == stdout || stream == stderr)
    {
        return 0;
    }

    /*
     * TODO: Implement when filesystem is available.
     */
    return 0;
}

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    /*
     * TODO: Implement when filesystem is available.
     */
    (void)ptr;
    (void)size;
    (void)nmemb;
    (void)stream;
    return 0;
}

size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    size_t i;
    size_t total = size * nmemb;
    const char *p = (const char *)ptr;

    /* Handle stdout/stderr specially */
    if (stream == stdout || stream == stderr)
    {
        for (i = 0; i < total; i++)
        {
            putchar(p[i]);
        }
        return nmemb;
    }

    /*
     * TODO: Implement for regular files when filesystem is available.
     */
    return 0;
}

int fseek(FILE *stream, long offset, int whence)
{
    /*
     * TODO: Implement when filesystem is available.
     */
    (void)stream;
    (void)offset;
    (void)whence;
    return -1;
}

long ftell(FILE *stream)
{
    /*
     * TODO: Implement when filesystem is available.
     */
    (void)stream;
    return -1;
}

int feof(FILE *stream)
{
    if (stream == NULL)
    {
        return 0;
    }
    return stream->eof;
}

int ferror(FILE *stream)
{
    if (stream == NULL)
    {
        return 0;
    }
    return stream->error;
}

void clearerr(FILE *stream)
{
    if (stream != NULL)
    {
        stream->eof = 0;
        stream->error = 0;
    }
}

void rewind(FILE *stream)
{
    if (stream != NULL)
    {
        fseek(stream, 0, SEEK_SET);
        clearerr(stream);
    }
}

int fputc(int c, FILE *stream)
{
    if (stream == stdout || stream == stderr)
    {
        return putchar(c);
    }

    /*
     * TODO: Implement for regular files when filesystem is available.
     */
    return EOF;
}

int fputs(const char *s, FILE *stream)
{
    while (*s)
    {
        if (fputc(*s++, stream) == EOF)
        {
            return EOF;
        }
    }
    return 0;
}

int fprintf(FILE *stream, const char *format, ...)
{
    unsigned int *args = (unsigned int *)&format + 1;

    if (stream == stdout || stream == stderr)
    {
        return vprintf_internal(output_char_stdout, NULL, format, args);
    }

    /*
     * TODO: Implement for regular files when filesystem is available.
     */
    return 0;
}

int fgetc(FILE *stream)
{
    if (stream == stdin)
    {
        return getchar();
    }

    /*
     * TODO: Implement for regular files when filesystem is available.
     */
    return EOF;
}

char *fgets(char *s, int size, FILE *stream)
{
    int c;
    char *p = s;
    int count = 0;

    if (size <= 0)
    {
        return NULL;
    }

    while (count < size - 1)
    {
        c = fgetc(stream);
        
        if (c == EOF)
        {
            if (count == 0)
            {
                return NULL;
            }
            break;
        }

        *p++ = (char)c;
        count++;

        if (c == '\n')
        {
            break;
        }
    }

    *p = '\0';
    return s;
}

int fflush(FILE *stream)
{
    /*
     * No buffering in this implementation, so nothing to flush.
     */
    (void)stream;
    return 0;
}
