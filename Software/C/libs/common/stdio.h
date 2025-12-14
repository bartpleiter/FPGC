#ifndef STDIO_H
#define STDIO_H

/*
 * Standard I/O Library
 * Minimal implementation for FPGC.
 * 
 * Note: File I/O functions are stubs until filesystem is implemented.
 * Printf-family supports: %d, %i, %u, %x, %X, %c, %s, %p, %%
 * No floating-point support (no FPU).
 */

#include "libs/common/stddef.h"

/* End of file indicator */
#define EOF (-1)

/*
 * FILE structure - placeholder for future filesystem implementation.
 * Currently only supports stdin, stdout, stderr as special values.
 */
typedef struct _FILE
{
    int fd;         /* File descriptor */
    int flags;      /* File mode flags */
    int eof;        /* EOF indicator */
    int error;      /* Error indicator */
} FILE;

/* Standard streams (placeholder pointers) */
extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

/* Stream constants */
#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

/* File opening modes */
#define _STDIO_READ   0x01
#define _STDIO_WRITE  0x02
#define _STDIO_APPEND 0x04
#define _STDIO_BINARY 0x08

/* Character output (basic) */

/**
 * Output a single character to stdout (UART).
 * @param c Character to output
 * @return Character written, or EOF on error
 */
int putchar(int c);

/**
 * Output a string to stdout followed by newline.
 * @param s String to output
 * @return Non-negative on success, EOF on error
 */
int puts(const char *s);

/* Character input */

/**
 * Read a character from stdin.
 * Note: Blocking until character available.
 * TODO: Implement when keyboard/UART input is available.
 * @return Character read, or EOF
 */
int getchar(void);

/* Formatted output */

/**
 * Print formatted output to stdout.
 * Supports: %d, %i, %u, %x, %X, %c, %s, %p, %%
 * Field width and precision supported.
 * @param format Format string
 * @param ... Arguments
 * @return Number of characters printed
 */
int printf(const char *format, ...);

/**
 * Print formatted output to string.
 * @param str Destination buffer
 * @param format Format string
 * @param ... Arguments
 * @return Number of characters written (excluding null)
 */
int sprintf(char *str, const char *format, ...);

/**
 * Print formatted output to string with size limit.
 * @param str Destination buffer
 * @param size Buffer size
 * @param format Format string
 * @param ... Arguments
 * @return Number of characters that would be written (excluding null)
 */
int snprintf(char *str, size_t size, const char *format, ...);

/* File operations (stubs for future implementation) */

/**
 * Open a file.
 * TODO: Implement when filesystem is available.
 * @param pathname File path
 * @param mode Opening mode ("r", "w", "a", etc.)
 * @return FILE pointer, or NULL on error
 */
FILE *fopen(const char *pathname, const char *mode);

/**
 * Close a file.
 * @param stream File pointer
 * @return 0 on success, EOF on error
 */
int fclose(FILE *stream);

/**
 * Read from a file.
 * @param ptr Destination buffer
 * @param size Size of each element
 * @param nmemb Number of elements
 * @param stream File pointer
 * @return Number of elements read
 */
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);

/**
 * Write to a file.
 * @param ptr Source buffer
 * @param size Size of each element
 * @param nmemb Number of elements
 * @param stream File pointer
 * @return Number of elements written
 */
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);

/**
 * Seek to position in file.
 * @param stream File pointer
 * @param offset Offset from origin
 * @param whence Origin: SEEK_SET, SEEK_CUR, SEEK_END
 * @return 0 on success, non-zero on error
 */
int fseek(FILE *stream, long offset, int whence);

/**
 * Get current position in file.
 * @param stream File pointer
 * @return Current position, or -1 on error
 */
long ftell(FILE *stream);

/**
 * Test end-of-file indicator.
 * @param stream File pointer
 * @return Non-zero if EOF, zero otherwise
 */
int feof(FILE *stream);

/**
 * Test error indicator.
 * @param stream File pointer
 * @return Non-zero if error, zero otherwise
 */
int ferror(FILE *stream);

/**
 * Clear error and EOF indicators.
 * @param stream File pointer
 */
void clearerr(FILE *stream);

/**
 * Rewind file to beginning.
 * @param stream File pointer
 */
void rewind(FILE *stream);

/* Seek origins */
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

/* Character output to stream */

/**
 * Output character to stream.
 * @param c Character
 * @param stream File pointer
 * @return Character written, or EOF on error
 */
int fputc(int c, FILE *stream);

/**
 * Output string to stream.
 * @param s String
 * @param stream File pointer
 * @return Non-negative on success, EOF on error
 */
int fputs(const char *s, FILE *stream);

/**
 * Print formatted output to stream.
 * @param stream File pointer
 * @param format Format string
 * @param ... Arguments
 * @return Number of characters printed
 */
int fprintf(FILE *stream, const char *format, ...);

/* Character input from stream */

/**
 * Read character from stream.
 * @param stream File pointer
 * @return Character read, or EOF
 */
int fgetc(FILE *stream);

/**
 * Read string from stream.
 * @param s Destination buffer
 * @param size Buffer size
 * @param stream File pointer
 * @return s on success, NULL on error or EOF
 */
char *fgets(char *s, int size, FILE *stream);

/* Buffer control (stubs) */

/**
 * Flush stream buffer.
 * @param stream File pointer (NULL to flush all)
 * @return 0 on success, EOF on error
 */
int fflush(FILE *stream);

#endif /* STDIO_H */
