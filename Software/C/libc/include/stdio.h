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

/* Standard streams — use macros so that &__XXX_file uses PC-relative
 * addressing in PIC mode, avoiding broken absolute pointers in .data.  */
extern struct __stdio_file __stdin_file;
extern struct __stdio_file __stdout_file;
extern struct __stdio_file __stderr_file;

#define stdin  ((FILE *)&__stdin_file)
#define stdout ((FILE *)&__stdout_file)
#define stderr ((FILE *)&__stderr_file)

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
int rename(const char *oldpath, const char *newpath);

#endif /* _STDIO_H */
