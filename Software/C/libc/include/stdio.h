/*
 * stdio.h — Standard I/O for B32P3/FPGC
 *
 * Provides printf family (formatted output), putchar/puts, and basic FILE I/O.
 * The FILE implementation is minimal — suited for bare-metal/OS with a small
 * number of open files.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _STDIO_H
#define _STDIO_H

#include <stddef.h>
#include <stdarg.h>

#define EOF (-1)

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

/* Buffering modes (for setvbuf — not implemented, defined for compatibility) */
#define _IOFBF 0
#define _IOLBF 1
#define _IONBF 2

/* FILE is opaque — internal structure defined in stdio implementation */
typedef struct __stdio_file FILE;

/* Standard streams */
extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

/* Formatted output */
int printf(const char *format, ...);
int fprintf(FILE *stream, const char *format, ...);
int sprintf(char *str, const char *format, ...);
int snprintf(char *str, size_t size, const char *format, ...);
int vprintf(const char *format, va_list ap);
int vfprintf(FILE *stream, const char *format, va_list ap);
int vsprintf(char *str, const char *format, va_list ap);
int vsnprintf(char *str, size_t size, const char *format, va_list ap);

/* Character I/O */
int fputc(int c, FILE *stream);
int fputs(const char *s, FILE *stream);
int putchar(int c);
int puts(const char *s);
int fgetc(FILE *stream);
int getchar(void);
int ungetc(int c, FILE *stream);

/* Direct I/O */
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);

/* File operations */
FILE *fopen(const char *path, const char *mode);
int   fclose(FILE *stream);
int   fflush(FILE *stream);
int   fseek(FILE *stream, long offset, int whence);
long  ftell(FILE *stream);
void  rewind(FILE *stream);
int   feof(FILE *stream);
int   ferror(FILE *stream);
void  clearerr(FILE *stream);

/* Formatted input (basic) */
int sscanf(const char *str, const char *format, ...);

/* Remove file */
int remove(const char *pathname);

#endif /* _STDIO_H */
