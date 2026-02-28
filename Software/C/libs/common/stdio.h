#ifndef STDIO_H
#define STDIO_H

// FPGC stdio library
// Provides standard FILE I/O wrapping BRFS syscalls.
//
// FILE is void. FILE* points to internal stdio_file struct.
// stdout sentinel: STDIO_STDOUT = (FILE*)(-1)

#define FILE void
#define EOF  (-1)
#define STDIO_STDOUT ((FILE*)(-1))

// fpos_t — matches B32CC's __SMALLER_C__ definition in smlrc.c
#ifndef FPOS_T_DEFINED
#define FPOS_T_DEFINED
struct fpos_t_
{
  union
  {
    unsigned short halves[2];
    int align;
  } u;
};
#define fpos_t struct fpos_t_
#endif

FILE* fopen(char* path, char* mode);
int fclose(FILE* f);
int fgetc(FILE* f);
int fputc(int c, FILE* f);
int fputs(char* s, FILE* f);
int putchar(int c);
int puts(char* s);

// Position tracking (used by B32CC's GenUpdateFrameSize)
int fgetpos(FILE* f, fpos_t* pos);
int fsetpos(FILE* f, fpos_t* pos);

// printf family — variadics with B32CC's MaxParams=16 cap
int printf(char* format, ...);
int vprintf(char* format, void* vl);
int vfprintf(FILE* f, char* format, void* vl);

// Declared for compatibility
int fprintf(FILE* f, char* format, ...);
int sprintf(char* buf, char* format, ...);
int vsprintf(char* buf, char* format, void* vl);

void exit(int code);

#endif // STDIO_H
