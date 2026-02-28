// FPGC stdio implementation
// Wraps BRFS syscalls for FILE I/O with position tracking.
// Requires: USER_SYSCALL (for sys_fs_* and sys_print_*).
//
// FILE is #defined as void. FILE* points to a stdio_file struct
// from a small static pool. Special stdout sentinel = STDIO_STDOUT.

// TODO: this lib is currently just hacked together to support running B32CC on FPGC.
//  it should be cleaned up and properly implemented as a general stdio library for FPGC in the future.

// ---------- internal FILE state ----------

// Max simultaneously open files (B32CC uses ~5: 1 output + up to 4 includes)
#define STDIO_MAX_FILES 8

struct stdio_file
{
  int fd;        // BRFS file descriptor, -1 if unused
  int position;  // current word offset in file
  int mode;      // 0 = read, 1 = write
};

static struct stdio_file stdio_files[STDIO_MAX_FILES];
static int stdio_initialized = 0;

static void stdio_init()
{
  int i;
  if (stdio_initialized)
    return;
  for (i = 0; i < STDIO_MAX_FILES; i++)
    stdio_files[i].fd = -1;
  stdio_initialized = 1;
}

static struct stdio_file* stdio_alloc()
{
  int i;
  stdio_init();
  for (i = 0; i < STDIO_MAX_FILES; i++)
  {
    if (stdio_files[i].fd == -1)
      return &stdio_files[i];
  }
  return (struct stdio_file*)0;
}

// ---------- path resolution ----------

// BRFS requires absolute paths. If the given path is relative (doesn't start
// with '/'), prepend the BDOS shell's CWD. Returns a pointer to either the
// original path (if already absolute) or an internal static buffer.
#define STDIO_PATH_BUF_SIZE 192

static char stdio_path_buf[STDIO_PATH_BUF_SIZE];

static char* stdio_resolve_path(char* path)
{
  char* cwd;
  int cwd_len;
  int path_len;

  if (!path || path[0] == '/')
    return path;

  cwd = sys_shell_getcwd();
  if (!cwd || cwd[0] == 0)
    return path;

  cwd_len = strlen(cwd);
  path_len = strlen(path);

  // Check buffer overflow
  if (cwd_len + 1 + path_len + 1 > STDIO_PATH_BUF_SIZE)
    return path;

  strcpy(stdio_path_buf, cwd);
  // Add separator if CWD doesn't end with '/'
  if (cwd_len > 0 && cwd[cwd_len - 1] != '/')
  {
    stdio_path_buf[cwd_len] = '/';
    strcpy(stdio_path_buf + cwd_len + 1, path);
  }
  else
  {
    strcpy(stdio_path_buf + cwd_len, path);
  }

  return stdio_path_buf;
}

// ---------- fopen / fclose ----------

FILE* fopen(char* path, char* mode)
{
  struct stdio_file* sf;
  int fd;

  stdio_init();

  // Resolve relative paths to absolute using CWD
  path = stdio_resolve_path(path);

  sf = stdio_alloc();
  if (!sf)
    return (FILE*)0;

  if (mode[0] == 'w')
  {
    sys_fs_delete(path);
    if (sys_fs_create(path) < 0)
      return (FILE*)0;
    fd = sys_fs_open(path);
    if (fd < 0)
      return (FILE*)0;
    sf->mode = 1;
  }
  else
  {
    fd = sys_fs_open(path);
    if (fd < 0)
      return (FILE*)0;
    sf->mode = 0;
  }

  sf->fd = fd;
  sf->position = 0;
  return (FILE*)sf;
}

int fclose(FILE* f)
{
  struct stdio_file* sf;
  int result;

  if (!f || (int)f == -1)
    return -1;

  sf = (struct stdio_file*)f;
  result = sys_fs_close(sf->fd);
  sf->fd = -1;
  sf->position = 0;
  return result;
}

// ---------- character I/O ----------

int fgetc(FILE* f)
{
  struct stdio_file* sf;
  unsigned int word;
  int n;

  if (!f || (int)f == -1)
    return EOF;

  sf = (struct stdio_file*)f;
  n = sys_fs_read(sf->fd, &word, 1);
  if (n <= 0)
    return EOF;

  sf->position++;
  return (int)word;
}

int fputc(int c, FILE* f)
{
  struct stdio_file* sf;
  unsigned int word;

  if ((int)f == -1)
  {
    // stdout
    sys_print_char(c);
    return c;
  }

  if (!f)
    return EOF;

  sf = (struct stdio_file*)f;
  word = (unsigned int)c;
  if (sys_fs_write(sf->fd, &word, 1) <= 0)
    return EOF;

  sf->position++;
  return c;
}

int fputs(char* s, FILE* f)
{
  while (*s)
  {
    if (fputc(*s, f) == EOF)
      return EOF;
    s++;
  }
  return 0;
}

int putchar(int c)
{
  sys_print_char(c);
  return c;
}

int puts(char* s)
{
  sys_print_str(s);
  sys_print_char('\n');
  return 0;
}

// ---------- position tracking ----------

int fgetpos(FILE* f, fpos_t* pos)
{
  struct stdio_file* sf;

  if (!f || (int)f == -1 || !pos)
    return -1;

  sf = (struct stdio_file*)f;
  pos->u.align = sf->position;
  return 0;
}

int fsetpos(FILE* f, fpos_t* pos)
{
  struct stdio_file* sf;

  if (!f || (int)f == -1 || !pos)
    return -1;

  sf = (struct stdio_file*)f;
  sf->position = pos->u.align;
  return sys_fs_seek(sf->fd, sf->position);
}

// ---------- formatted output ----------

static void stdio_emit(FILE* f, int c, int* count)
{
  fputc(c, f);
  (*count)++;
}

static void stdio_emit_str(FILE* f, char* s, int* count)
{
  while (*s)
  {
    fputc(*s, f);
    (*count)++;
    s++;
  }
}

// flags: bit 0=zero-pad, bit 2=force-sign(+), bit 3=uppercase-hex
static void stdio_emit_uint(FILE* f, unsigned int val, int base, int width,
                            int flags, int is_signed, int* count)
{
  char buf[12];
  int i;
  int neg;
  int len;
  int pad_char;
  int show_plus;
  unsigned int uval;

  neg = 0;
  show_plus = flags & 4;

  if (is_signed && ((int)val < 0))
  {
    neg = 1;
    uval = (unsigned int)(-(int)val);
  }
  else
  {
    uval = val;
  }

  i = 0;
  if (uval == 0)
  {
    buf[i++] = '0';
  }
  else
  {
    while (uval > 0)
    {
      int digit;
      digit = uval % base;
      if (digit < 10)
        buf[i++] = '0' + digit;
      else if (flags & 8)
        buf[i++] = 'A' + digit - 10;
      else
        buf[i++] = 'a' + digit - 10;
      uval = uval / base;
    }
  }

  len = i;
  if (neg || show_plus)
    len++;

  pad_char = (flags & 1) ? '0' : ' ';

  if ((flags & 1) && (neg || show_plus))
    stdio_emit(f, neg ? '-' : '+', count);

  while (len < width)
  {
    stdio_emit(f, pad_char, count);
    len++;
  }

  if (!(flags & 1) && (neg || show_plus))
    stdio_emit(f, neg ? '-' : '+', count);

  while (i > 0)
  {
    i--;
    stdio_emit(f, buf[i], count);
  }
}

// Core vfprintf: supports %%, %c, %s, %d, %i, %u, %o, %x, %X
// Flags: 0 (zero-pad), + (force sign)
// Width: numeric
int vfprintf(FILE* f, char* format, void* vl)
{
  int count;
  int* ap;
  char ch;
  int flags;
  int width;

  count = 0;
  ap = (int*)vl;

  while ((ch = *format++) != 0)
  {
    if (ch != '%')
    {
      stdio_emit(f, ch, &count);
      continue;
    }

    flags = 0;
    for (;;)
    {
      ch = *format;
      if (ch == '0')      { flags = flags | 1; format++; }
      else if (ch == '+') { flags = flags | 4; format++; }
      else if (ch == '-') { flags = flags | 2; format++; }
      else break;
    }

    width = 0;
    while (*format >= '0' && *format <= '9')
    {
      width = width * 10 + (*format - '0');
      format++;
    }

    ch = *format++;
    if (ch == 'd' || ch == 'i')
    {
      stdio_emit_uint(f, (unsigned int)*ap, 10, width, flags, 1, &count);
      ap++;
    }
    else if (ch == 'u')
    {
      stdio_emit_uint(f, (unsigned int)*ap, 10, width, flags, 0, &count);
      ap++;
    }
    else if (ch == 'o')
    {
      stdio_emit_uint(f, (unsigned int)*ap, 8, width, flags, 0, &count);
      ap++;
    }
    else if (ch == 'x')
    {
      stdio_emit_uint(f, (unsigned int)*ap, 16, width, flags, 0, &count);
      ap++;
    }
    else if (ch == 'X')
    {
      stdio_emit_uint(f, (unsigned int)*ap, 16, width, flags | 8, 0, &count);
      ap++;
    }
    else if (ch == 'c')
    {
      stdio_emit(f, *ap, &count);
      ap++;
    }
    else if (ch == 's')
    {
      char* s;
      s = (char*)*ap;
      if (s)
        stdio_emit_str(f, s, &count);
      else
        stdio_emit_str(f, "(null)", &count);
      ap++;
    }
    else if (ch == '%')
    {
      stdio_emit(f, '%', &count);
    }
    else
    {
      stdio_emit(f, '%', &count);
      stdio_emit(f, ch, &count);
    }
  }

  return count;
}

int vprintf(char* format, void* vl)
{
  return vfprintf(STDIO_STDOUT, format, vl);
}

int printf(char* format, ...)
{
  void* vl;
  vl = &format + 1;
  return vfprintf(STDIO_STDOUT, format, vl);
}

int fprintf(FILE* f, char* format, ...)
{
  void* vl;
  vl = &format + 1;
  return vfprintf(f, format, vl);
}

int sprintf(char* buf, char* format, ...)
{
  // Not used by B32CC. Stub.
  return 0;
}

int vsprintf(char* buf, char* format, void* vl)
{
  // Not used by B32CC. Stub.
  return 0;
}

// ---------- exit ----------

void exit(int code)
{
  sys_exit(code);
}
