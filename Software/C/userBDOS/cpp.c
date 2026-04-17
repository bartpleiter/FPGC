/*
 * cpp.c — Minimal C preprocessor for FPGC self-hosting toolchain.
 *
 * Targets the subset of features actually used by libc/userlib/userBDOS:
 *   - #include <file>  / #include "file"     (search -I paths, then quote dir)
 *   - #define NAME [body]                    (object-like)
 *   - #define NAME(a, b, ...) body           (function-like)
 *   - #ifdef / #ifndef / #else / #endif      (no #if expression evaluation)
 *   - -D NAME[=VAL] command-line defines
 *   - -I PATH        include search paths (multiple allowed)
 *   - C-style /* ... *\/ and // ... comments stripped
 *   - Backslash-newline line continuation
 *
 * NOT supported (unused in the codebase outside of doom):
 *   - #if EXPR / #elif / #undef / #pragma / #error / #line
 *   - Token pasting (##) and stringification (#)
 *   - __FILE__ / __LINE__ / __DATE__ / __TIME__ / __COUNTER__
 *   - Variadic macros (...)
 *   - Computed includes (#include MACRO)
 *
 * Output mirrors `cpp -P` (no line markers).
 *
 * Defining CPP_HOST builds a host (gcc/Linux) variant with stdio for testing.
 */

#ifdef CPP_HOST
  #include <stdio.h>
  #include <stdlib.h>
  #include <string.h>
  #include <ctype.h>
#else
  #include <stdio.h>
  #include <stdlib.h>
  #include <string.h>
  #include <ctype.h>
  #include <syscall.h>
#endif

/*===========================================================================*/
/*  Limits and buffer sizes                                                  */
/*===========================================================================*/

#define MAX_MACROS         1024
#define MAX_MACRO_PARAMS   16
#define MAX_INCLUDE_DIRS   16
#define MAX_INCLUDE_DEPTH  16
#define MAX_COND_DEPTH     64
#define LINE_BUF_BYTES     (4 * 1024)
/* expand_pass uses several stack-allocated buffers of this size and recurses,
 * so keep the total frame under what b32p3's 16-bit signed immediates can
 * encode for `sub r13 imm` (~32 KB). 4 KB per buffer × 6 ≈ 24 KB. */
#define EXPAND_BUF_BYTES   (4 * 1024)
#define ID_MAX_LEN         128

/* Per-macro string storage budget; lives in a single bump allocator. */
#define STR_POOL_BYTES     (1024 * 1024)

/*===========================================================================*/
/*  I/O abstraction                                                          */
/*===========================================================================*/

#ifdef CPP_HOST

#define MAX_HOST_FDS 16
static FILE *host_files[MAX_HOST_FDS];

static int host_open(const char *path)
{
  int i;
  FILE *f = fopen(path, "rb");
  if (!f) return -1;
  for (i = 0; i < MAX_HOST_FDS; i++)
  {
    if (host_files[i] == NULL) { host_files[i] = f; return i; }
  }
  fclose(f); return -1;
}

static int host_close(int fd)
{
  if (fd < 0 || fd >= MAX_HOST_FDS || host_files[fd] == NULL) return -1;
  fclose(host_files[fd]); host_files[fd] = NULL; return 0;
}

static int host_filesize_bytes(int fd)
{
  long pos, end;
  if (fd < 0 || fd >= MAX_HOST_FDS || host_files[fd] == NULL) return -1;
  pos = ftell(host_files[fd]);
  fseek(host_files[fd], 0, SEEK_END);
  end = ftell(host_files[fd]);
  fseek(host_files[fd], pos, SEEK_SET);
  return (int)end;
}

static int host_read_bytes(int fd, void *buf, int n)
{
  if (fd < 0 || fd >= MAX_HOST_FDS || host_files[fd] == NULL) return -1;
  return (int)fread(buf, 1, (size_t)n, host_files[fd]);
}

#define IO_OPEN(p)            host_open(p)
#define IO_CLOSE(fd)          host_close(fd)
#define IO_FILESIZE_BYTES(fd) host_filesize_bytes(fd)
#define IO_READ_BYTES(fd, b, n) host_read_bytes(fd, b, n)
#define IO_HEAP_ALLOC(n)      malloc((size_t)(n))
#define IO_PRINT_ERR(s)       fputs((s), stderr)

#else /* BDOS build: file storage is word-based; we read words then unpack. */

#define IO_OPEN(p)            sys_fs_open(p)
#define IO_CLOSE(fd)          sys_fs_close(fd)
/* IO_FILESIZE_BYTES / IO_READ_BYTES are emulated below using sys_fs_*. */
#define IO_HEAP_ALLOC(n)      sys_heap_alloc(n)
#define IO_PRINT_ERR(s)       sys_print_str(s)

static int bdos_filesize_bytes(int fd)
{
  /* sys_fs_filesize returns words; we don't know the exact byte length
   * (last-word padding is unknown). For source code that's fine — trailing
   * NULs are harmless because we treat them as whitespace.
   */
  int w = sys_fs_filesize(fd);
  if (w < 0) return -1;
  return w * 4;
}

static int bdos_read_bytes(int fd, void *buf, int nbytes)
{
  /* Read full words, then byte-swap from BE word to native byte order. */
  unsigned int *wbuf = (unsigned int *)buf;
  int nwords = (nbytes + 3) / 4;
  int got = sys_fs_read(fd, wbuf, nwords);
  int i;
  if (got < 0) return -1;
  /* big-endian unpack into byte buffer */
  for (i = 0; i < got; i++)
  {
    unsigned int w = wbuf[i];
    unsigned char *p = (unsigned char *)buf + i * 4;
    p[0] = (unsigned char)(w >> 24);
    p[1] = (unsigned char)(w >> 16);
    p[2] = (unsigned char)(w >> 8);
    p[3] = (unsigned char)(w);
  }
  return got * 4;
}

#define IO_FILESIZE_BYTES(fd)    bdos_filesize_bytes(fd)
#define IO_READ_BYTES(fd, b, n)  bdos_read_bytes(fd, b, n)

#endif

/*===========================================================================*/
/*  Globals                                                                  */
/*===========================================================================*/

static char *str_pool;
static int   str_pool_pos;
static int   str_pool_size;

typedef struct {
  char *name;
  char *body;          /* may be empty */
  int   nparams;       /* -1 = object-like; >=0 = function-like with N params */
  char *params[MAX_MACRO_PARAMS];
} Macro;

static Macro macros[MAX_MACROS];
static int   macro_count;

static char *include_dirs[MAX_INCLUDE_DIRS];
static int   include_dir_count;

static char *quote_dir_stack[MAX_INCLUDE_DEPTH];
static int   include_depth;

/* Conditional stack: cond_active[i] = is current branch outputting?
 * cond_taken[i]  = has any branch in this group been taken? */
static int cond_active[MAX_COND_DEPTH];
static int cond_taken[MAX_COND_DEPTH];
static int cond_depth;

static FILE *out_fp;            /* host output stream */
#ifndef CPP_HOST
static int   out_fd;            /* BDOS output fd */
static unsigned int out_word_buf;
static int          out_word_fill; /* bytes pending in out_word_buf (0..3) */
#endif

static int has_error;
static const char *cur_filename;
static int          cur_line;

/*===========================================================================*/
/*  Output                                                                   */
/*===========================================================================*/

#ifdef CPP_HOST
static void out_write(const char *s, int n)
{
  if (out_fp) fwrite(s, 1, (size_t)n, out_fp);
  else fwrite(s, 1, (size_t)n, stdout);
}
static void out_flush(void) { if (out_fp) fflush(out_fp); else fflush(stdout); }
#else
static void out_write(const char *s, int n)
{
  int i;
  for (i = 0; i < n; i++)
  {
    out_word_buf = (out_word_buf << 8) | (unsigned char)s[i];
    out_word_fill++;
    if (out_word_fill == 4)
    {
      sys_fs_write(out_fd, &out_word_buf, 1);
      out_word_buf = 0;
      out_word_fill = 0;
    }
  }
}
static void out_flush(void)
{
  /* Pad final word with zeros (treated as trailing NULs by readers). */
  if (out_word_fill > 0)
  {
    out_word_buf <<= 8 * (4 - out_word_fill);
    sys_fs_write(out_fd, &out_word_buf, 1);
    out_word_buf = 0;
    out_word_fill = 0;
  }
}
#endif

static void out_str(const char *s)
{
  int n = (int)strlen(s);
  out_write(s, n);
}

/*===========================================================================*/
/*  Diagnostics                                                              */
/*===========================================================================*/

static void emsg(const char *s)
{
  char buf[64];
  IO_PRINT_ERR("cpp: ");
  if (cur_filename)
  {
    IO_PRINT_ERR(cur_filename);
    snprintf(buf, sizeof(buf), ":%d: ", cur_line);
    IO_PRINT_ERR(buf);
  }
  IO_PRINT_ERR(s);
  IO_PRINT_ERR("\n");
  has_error = 1;
}

static void emsg2(const char *a, const char *b)
{
  char buf[64];
  IO_PRINT_ERR("cpp: ");
  if (cur_filename)
  {
    IO_PRINT_ERR(cur_filename);
    snprintf(buf, sizeof(buf), ":%d: ", cur_line);
    IO_PRINT_ERR(buf);
  }
  IO_PRINT_ERR(a);
  IO_PRINT_ERR(b);
  IO_PRINT_ERR("\n");
  has_error = 1;
}

/*===========================================================================*/
/*  String pool                                                              */
/*===========================================================================*/

static char *pool_strndup(const char *s, int n)
{
  char *r;
  if (str_pool_pos + n + 1 > str_pool_size)
  {
    emsg("string pool exhausted");
    return NULL;
  }
  r = str_pool + str_pool_pos;
  memcpy(r, s, (size_t)n);
  r[n] = '\0';
  str_pool_pos += n + 1;
  return r;
}

static char *pool_strdup(const char *s)
{
  return pool_strndup(s, (int)strlen(s));
}

/*===========================================================================*/
/*  Macro table                                                              */
/*===========================================================================*/

static int macro_lookup(const char *name, int nlen)
{
  int i;
  for (i = 0; i < macro_count; i++)
  {
    if ((int)strlen(macros[i].name) == nlen
        && memcmp(macros[i].name, name, (size_t)nlen) == 0)
      return i;
  }
  return -1;
}

static int macro_define(const char *name, int nlen,
                        const char *body,
                        int nparams, char **params)
{
  int idx = macro_lookup(name, nlen);
  Macro *m;
  if (idx < 0)
  {
    if (macro_count >= MAX_MACROS) { emsg("too many macros"); return -1; }
    idx = macro_count++;
    macros[idx].name = pool_strndup(name, nlen);
    if (!macros[idx].name) return -1;
  }
  m = &macros[idx];
  m->body = pool_strdup(body);
  if (!m->body) return -1;
  m->nparams = nparams;
  if (nparams > 0)
  {
    int i;
    for (i = 0; i < nparams; i++) m->params[i] = params[i];
  }
  return idx;
}

/*===========================================================================*/
/*  Helpers                                                                  */
/*===========================================================================*/

static int is_id_start(int c)
{
  return (c == '_') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static int is_id_cont(int c)
{
  return is_id_start(c) || (c >= '0' && c <= '9');
}

/* Skip horizontal whitespace (spaces and tabs only) starting at p; return new p. */
static const char *skip_hws(const char *p)
{
  while (*p == ' ' || *p == '\t') p++;
  return p;
}

/*===========================================================================*/
/*  File loading                                                             */
/*===========================================================================*/

/* Load file into a heap buffer, NUL-terminated. */
static char *load_file(const char *path, int *out_len)
{
  int fd, sz, got;
  char *buf;
  fd = IO_OPEN(path);
  if (fd < 0) return NULL;
  sz = IO_FILESIZE_BYTES(fd);
  if (sz < 0) { IO_CLOSE(fd); return NULL; }
  buf = (char *)IO_HEAP_ALLOC(sz + 1);
  if (!buf) { IO_CLOSE(fd); return NULL; }
  got = IO_READ_BYTES(fd, buf, sz);
  IO_CLOSE(fd);
  if (got < 0) return NULL;
  buf[got] = '\0';
  if (out_len) *out_len = got;
  return buf;
}

/* Resolve include path. quoted=1 for "...", 0 for <...>. */
static int resolve_include(const char *name, int quoted,
                           char *out, int outsz)
{
  int i;
  /* For "...", search the directory of the including file first. */
  if (quoted && include_depth > 0)
  {
    const char *dir = quote_dir_stack[include_depth - 1];
    if (dir && dir[0])
    {
      snprintf(out, (size_t)outsz, "%s/%s", dir, name);
      {
        int fd = IO_OPEN(out);
        if (fd >= 0) { IO_CLOSE(fd); return 0; }
      }
    }
  }
  /* Search -I paths. */
  for (i = 0; i < include_dir_count; i++)
  {
    snprintf(out, (size_t)outsz, "%s/%s", include_dirs[i], name);
    {
      int fd = IO_OPEN(out);
      if (fd >= 0) { IO_CLOSE(fd); return 0; }
    }
  }
  /* Last resort: try the name as-is. */
  {
    int fd = IO_OPEN(name);
    if (fd >= 0)
    {
      IO_CLOSE(fd);
      strncpy(out, name, (size_t)outsz);
      out[outsz - 1] = '\0';
      return 0;
    }
  }
  return -1;
}

/* Extract the directory part of a path into dst (may be empty). */
static void path_dirname(const char *path, char *dst, int dstsz)
{
  int n = (int)strlen(path);
  int i;
  for (i = n - 1; i >= 0; i--) if (path[i] == '/') break;
  if (i < 0) { dst[0] = '\0'; return; }
  if (i >= dstsz) i = dstsz - 1;
  memcpy(dst, path, (size_t)i);
  dst[i] = '\0';
}

/*===========================================================================*/
/*  Logical-line reader                                                      */
/*===========================================================================*/

/* Returns pointer past the consumed source slice, fills `out` with the
 * logical line (with backslash-newlines joined). out is NUL-terminated.
 * Comments are NOT stripped here. Sets *lines_consumed to physical lines
 * eaten (so cur_line tracks correctly).
 *
 * Returns NULL if at EOF.
 */
static const char *read_logical_line(const char *src, char *out, int outsz,
                                     int *lines_consumed)
{
  int outpos = 0;
  int lines = 0;
  if (*src == '\0') return NULL;
  while (*src != '\0')
  {
    if (*src == '\\' && src[1] == '\n')
    {
      /* line continuation: consume both, do not emit */
      src += 2;
      lines++;
      continue;
    }
    if (*src == '\n')
    {
      src++;
      lines++;
      break;
    }
    if (outpos >= outsz - 1)
    {
      emsg("line too long");
      break;
    }
    out[outpos++] = *src++;
  }
  out[outpos] = '\0';
  *lines_consumed = lines == 0 ? 1 : lines;
  return src;
}

/*===========================================================================*/
/*  Comment stripping                                                        */
/*===========================================================================*/

/* In-place comment stripping that respects strings and chars.
 * `in_block_p` is an in/out flag: if non-zero on entry, we start inside an
 * unterminated block comment carried from a previous line. On exit, set if
 * the block comment is still open at end of line.
 * Block comments become a single space (per ISO C). Line comments become
 * end-of-string. */
static void strip_comments(char *s, int *in_block_p)
{
  char *r = s;
  char *w = s;
  if (*in_block_p)
  {
    /* Skip until we find a closing star-slash; if not found, line is empty. */
    while (*r && !(r[0] == '*' && r[1] == '/')) r++;
    if (*r) { r += 2; *in_block_p = 0; *w++ = ' '; }
    else { *w = '\0'; return; }
  }
  while (*r)
  {
    if (*r == '"' || *r == '\'')
    {
      char q = *r;
      *w++ = *r++;
      while (*r && *r != q)
      {
        if (*r == '\\' && r[1])
        {
          *w++ = *r++;
          *w++ = *r++;
          continue;
        }
        *w++ = *r++;
      }
      if (*r) *w++ = *r++;
      continue;
    }
    if (r[0] == '/' && r[1] == '/')
    {
      *r = '\0';
      break;
    }
    if (r[0] == '/' && r[1] == '*')
    {
      r += 2;
      while (*r && !(r[0] == '*' && r[1] == '/')) r++;
      if (*r) { r += 2; *w++ = ' '; }
      else { *in_block_p = 1; *w++ = ' '; *w = '\0'; return; }
      continue;
    }
    *w++ = *r++;
  }
  *w = '\0';
}

/*===========================================================================*/
/*  Macro expansion                                                          */
/*===========================================================================*/

/* Substitute parameter references in `body` with corresponding `args`.
 * Result is appended to dst at *dpos (which is updated). dstsz is the
 * total size of dst. Returns 0 on success, -1 on overflow.
 */
static int substitute_params(const char *body,
                             int nparams, char **params, char **args,
                             char *dst, int *dpos, int dstsz)
{
  const char *p = body;
  int dp = *dpos;
  while (*p)
  {
    if (is_id_start((unsigned char)*p))
    {
      const char *q = p;
      int n;
      int i, found = -1;
      while (is_id_cont((unsigned char)*q)) q++;
      n = (int)(q - p);
      for (i = 0; i < nparams; i++)
      {
        if ((int)strlen(params[i]) == n
            && memcmp(params[i], p, (size_t)n) == 0)
        {
          found = i; break;
        }
      }
      if (found >= 0)
      {
        const char *a = args[found];
        int alen = (int)strlen(a);
        if (dp + alen >= dstsz) return -1;
        memcpy(dst + dp, a, (size_t)alen);
        dp += alen;
      }
      else
      {
        if (dp + n >= dstsz) return -1;
        memcpy(dst + dp, p, (size_t)n);
        dp += n;
      }
      p = q;
    }
    else
    {
      if (dp + 1 >= dstsz) return -1;
      dst[dp++] = *p++;
    }
  }
  *dpos = dp;
  return 0;
}

/* Expand all macros in `in`, write result to `out`. Returns 1 if any
 * expansion occurred (so caller may rescan). On error returns -1.
 *
 * Hide list `hide` is a comma-separated list of macro names that must
 * NOT be expanded (to prevent infinite recursion of self-referential
 * macros, per ISO C "blue paint" rule).
 */
static int expand_pass(const char *in, char *out, int outsz,
                       const char *hide)
{
  const char *p = in;
  int op = 0;
  int did = 0;
  while (*p)
  {
    /* Skip strings and char literals untouched. */
    if (*p == '"' || *p == '\'')
    {
      char q = *p;
      if (op + 1 >= outsz) return -1;
      out[op++] = *p++;
      while (*p && *p != q)
      {
        if (*p == '\\' && p[1])
        {
          if (op + 2 >= outsz) return -1;
          out[op++] = *p++;
          out[op++] = *p++;
          continue;
        }
        if (op + 1 >= outsz) return -1;
        out[op++] = *p++;
      }
      if (*p) { if (op + 1 >= outsz) return -1; out[op++] = *p++; }
      continue;
    }
    if (is_id_start((unsigned char)*p))
    {
      const char *id = p;
      int idlen;
      int midx;
      while (is_id_cont((unsigned char)*p)) p++;
      idlen = (int)(p - id);

      /* Check hide list. */
      if (hide && hide[0])
      {
        const char *h = hide;
        int hidden = 0;
        while (*h)
        {
          const char *e = strchr(h, ',');
          int hlen = e ? (int)(e - h) : (int)strlen(h);
          if (hlen == idlen && memcmp(h, id, (size_t)idlen) == 0)
          { hidden = 1; break; }
          h += hlen + (e ? 1 : 0);
          if (!e) break;
        }
        if (hidden)
        {
          if (op + idlen >= outsz) return -1;
          memcpy(out + op, id, (size_t)idlen);
          op += idlen;
          continue;
        }
      }

      midx = macro_lookup(id, idlen);
      if (midx < 0)
      {
        if (op + idlen >= outsz) return -1;
        memcpy(out + op, id, (size_t)idlen);
        op += idlen;
        continue;
      }

      /* Object-like */
      if (macros[midx].nparams < 0)
      {
        const char *body = macros[midx].body;
        char hide2[ID_MAX_LEN * 4];
        char tmp[EXPAND_BUF_BYTES];
        int rc;
        /* Build hide list = old hide + this name */
        if (hide && hide[0])
          snprintf(hide2, sizeof(hide2), "%s,%s", hide, macros[midx].name);
        else
          snprintf(hide2, sizeof(hide2), "%s", macros[midx].name);
        rc = expand_pass(body, tmp, sizeof(tmp), hide2);
        if (rc < 0) return -1;
        {
          int n = (int)strlen(tmp);
          if (op + n >= outsz) return -1;
          memcpy(out + op, tmp, (size_t)n);
          op += n;
        }
        did = 1;
        continue;
      }

      /* Function-like: peek for '(' (skip ws including newlines... but our
       * input is one logical line, so just spaces/tabs). */
      {
        const char *q = p;
        char *args[MAX_MACRO_PARAMS];
        char argbuf[EXPAND_BUF_BYTES];
        int  argpos = 0;
        int  nargs = 0;
        int  depth;
        char tmp[EXPAND_BUF_BYTES];
        char hide2[ID_MAX_LEN * 4];
        int  rc;

        while (*q == ' ' || *q == '\t') q++;
        if (*q != '(')
        {
          /* Not a call: emit the identifier verbatim. */
          if (op + idlen >= outsz) return -1;
          memcpy(out + op, id, (size_t)idlen);
          op += idlen;
          continue;
        }
        q++; /* past '(' */

        /* Parse args: comma-separated, paren-balanced, skipping strings. */
        args[0] = argbuf;
        depth = 1;
        while (*q && depth > 0)
        {
          if (*q == '"' || *q == '\'')
          {
            char qc = *q;
            argbuf[argpos++] = *q++;
            while (*q && *q != qc)
            {
              if (*q == '\\' && q[1])
              { argbuf[argpos++] = *q++; argbuf[argpos++] = *q++; continue; }
              argbuf[argpos++] = *q++;
            }
            if (*q) argbuf[argpos++] = *q++;
            continue;
          }
          if (*q == '(') { depth++; argbuf[argpos++] = *q++; continue; }
          if (*q == ')')
          {
            depth--;
            if (depth == 0) { q++; break; }
            argbuf[argpos++] = *q++; continue;
          }
          if (*q == ',' && depth == 1)
          {
            argbuf[argpos++] = '\0';
            if (nargs + 1 >= MAX_MACRO_PARAMS)
            { emsg("too many macro args"); return -1; }
            nargs++;
            args[nargs] = &argbuf[argpos];
            q++;
            while (*q == ' ' || *q == '\t') q++;
            continue;
          }
          argbuf[argpos++] = *q++;
          if (argpos >= (int)sizeof(argbuf) - 4)
          { emsg("macro arg too long"); return -1; }
        }
        if (depth != 0) { emsg("unterminated macro call"); return -1; }
        argbuf[argpos++] = '\0';
        nargs++;

        /* Trim trailing ws on each arg. */
        {
          int i;
          for (i = 0; i < nargs; i++)
          {
            int L = (int)strlen(args[i]);
            while (L > 0 && (args[i][L-1] == ' ' || args[i][L-1] == '\t'))
              args[i][--L] = '\0';
            /* Trim leading ws */
            while (args[i][0] == ' ' || args[i][0] == '\t') args[i]++;
          }
        }

        /* Special-case: empty macro call FOO() with 0 params. */
        if (macros[midx].nparams == 0)
        {
          if (!(nargs == 1 && args[0][0] == '\0'))
          {
            emsg2("wrong arg count for macro ", macros[midx].name);
            return -1;
          }
          nargs = 0;
        }
        else if (nargs != macros[midx].nparams)
        {
          emsg2("wrong arg count for macro ", macros[midx].name);
          return -1;
        }

        /* Pre-expand arguments (so MACRO(M(x)) sees expanded x). */
        {
          char expanded_argbuf[EXPAND_BUF_BYTES];
          int  ep = 0;
          char *exargs[MAX_MACRO_PARAMS];
          int i;
          for (i = 0; i < nargs; i++)
          {
            char tmpa[EXPAND_BUF_BYTES];
            int rc2 = expand_pass(args[i], tmpa, sizeof(tmpa), hide);
            int alen;
            if (rc2 < 0) return -1;
            alen = (int)strlen(tmpa);
            if (ep + alen + 1 >= (int)sizeof(expanded_argbuf))
            { emsg("expansion buffer overflow"); return -1; }
            exargs[i] = expanded_argbuf + ep;
            memcpy(expanded_argbuf + ep, tmpa, (size_t)alen);
            ep += alen;
            expanded_argbuf[ep++] = '\0';
          }

          /* Substitute params in body. */
          {
            char subst[EXPAND_BUF_BYTES];
            int sp = 0;
            int rcs = substitute_params(macros[midx].body,
                                        macros[midx].nparams,
                                        macros[midx].params,
                                        exargs, subst, &sp, sizeof(subst));
            if (rcs < 0) { emsg("expansion buffer overflow"); return -1; }
            subst[sp] = '\0';

            /* Recursive expand of result with this name in hide list. */
            if (hide && hide[0])
              snprintf(hide2, sizeof(hide2), "%s,%s", hide, macros[midx].name);
            else
              snprintf(hide2, sizeof(hide2), "%s", macros[midx].name);
            rc = expand_pass(subst, tmp, sizeof(tmp), hide2);
            if (rc < 0) return -1;
            {
              int n = (int)strlen(tmp);
              if (op + n >= outsz) return -1;
              memcpy(out + op, tmp, (size_t)n);
              op += n;
            }
            did = 1;
          }
        }

        p = q;
        continue;
      }
    }
    if (op + 1 >= outsz) return -1;
    out[op++] = *p++;
  }
  out[op] = '\0';
  return did;
}

/*===========================================================================*/
/*  Directive handling                                                       */
/*===========================================================================*/

/* Forward */
static void process_text(const char *text, const char *filename);

static void handle_define(const char *line)
{
  const char *p = line;
  const char *name;
  int nlen;
  int nparams = -1;
  char *params[MAX_MACRO_PARAMS];
  char paramsbuf[ID_MAX_LEN * MAX_MACRO_PARAMS];
  int  pbuf_pos = 0;
  const char *body;

  p = skip_hws(p);
  if (!is_id_start((unsigned char)*p)) { emsg("bad #define"); return; }
  name = p;
  while (is_id_cont((unsigned char)*p)) p++;
  nlen = (int)(p - name);

  if (*p == '(')
  {
    /* Function-like — note: NO whitespace between name and '(' */
    p++;
    nparams = 0;
    while (*p)
    {
      const char *pn;
      int plen;
      while (*p == ' ' || *p == '\t') p++;
      if (*p == ')') { p++; break; }
      if (!is_id_start((unsigned char)*p))
      { emsg("bad macro param"); return; }
      pn = p;
      while (is_id_cont((unsigned char)*p)) p++;
      plen = (int)(p - pn);
      if (nparams >= MAX_MACRO_PARAMS)
      { emsg("too many macro params"); return; }
      if (pbuf_pos + plen + 1 > (int)sizeof(paramsbuf))
      { emsg("macro params too long"); return; }
      params[nparams] = paramsbuf + pbuf_pos;
      memcpy(paramsbuf + pbuf_pos, pn, (size_t)plen);
      paramsbuf[pbuf_pos + plen] = '\0';
      pbuf_pos += plen + 1;
      nparams++;
      while (*p == ' ' || *p == '\t') p++;
      if (*p == ',') { p++; continue; }
      if (*p == ')') { p++; break; }
      emsg("bad macro params"); return;
    }
  }

  body = skip_hws(p);
  /* Trim trailing whitespace on body. */
  {
    int blen = (int)strlen(body);
    while (blen > 0 && (body[blen-1] == ' ' || body[blen-1] == '\t')) blen--;
    {
      char buf[LINE_BUF_BYTES];
      if (blen >= (int)sizeof(buf)) { emsg("macro body too long"); return; }
      memcpy(buf, body, (size_t)blen);
      buf[blen] = '\0';
      /* Note: pool_strdup'd inside macro_define; pass params (which point
       * into paramsbuf on stack) — duplicate them too. */
      {
        char *poolp[MAX_MACRO_PARAMS];
        int i;
        for (i = 0; i < nparams; i++)
        {
          poolp[i] = pool_strdup(params[i]);
          if (!poolp[i]) return;
        }
        macro_define(name, nlen, buf, nparams, poolp);
      }
    }
  }
}

static int branch_active(void)
{
  int i;
  for (i = 0; i < cond_depth; i++) if (!cond_active[i]) return 0;
  return 1;
}

static void cond_push(int active)
{
  if (cond_depth >= MAX_COND_DEPTH) { emsg("conditional nesting too deep"); return; }
  cond_active[cond_depth] = active;
  cond_taken[cond_depth] = active;
  cond_depth++;
}

static void handle_ifdef(const char *line, int negate)
{
  const char *p = skip_hws(line);
  const char *name = p;
  int nlen;
  int defined;
  int active;
  while (is_id_cont((unsigned char)*p)) p++;
  nlen = (int)(p - name);
  if (nlen == 0) { emsg("missing identifier"); return; }
  defined = macro_lookup(name, nlen) >= 0;
  active = negate ? !defined : defined;
  /* But only mark branch active if outer is active. */
  if (cond_depth > 0)
  {
    int outer = 1, i;
    for (i = 0; i < cond_depth; i++) if (!cond_active[i]) outer = 0;
    if (!outer) active = 0;
  }
  cond_push(active);
}

static void handle_else(void)
{
  if (cond_depth == 0) { emsg("#else without #if"); return; }
  {
    int outer = 1, i;
    for (i = 0; i < cond_depth - 1; i++) if (!cond_active[i]) outer = 0;
    if (!outer || cond_taken[cond_depth - 1])
      cond_active[cond_depth - 1] = 0;
    else
    {
      cond_active[cond_depth - 1] = 1;
      cond_taken[cond_depth - 1] = 1;
    }
  }
}

static void handle_endif(void)
{
  if (cond_depth == 0) { emsg("#endif without #if"); return; }
  cond_depth--;
}

static void handle_include(const char *line)
{
  const char *p = skip_hws(line);
  char name[256];
  char path[512];
  char dir[256];
  int  nlen;
  int  quoted;
  char *txt;
  const char *saved_filename;
  int saved_line;

  if (*p != '"' && *p != '<') { emsg("bad #include"); return; }
  quoted = (*p == '"');
  {
    char close = quoted ? '"' : '>';
    p++;
    nlen = 0;
    while (*p && *p != close)
    {
      if (nlen >= (int)sizeof(name) - 1) { emsg("include path too long"); return; }
      name[nlen++] = *p++;
    }
    name[nlen] = '\0';
    if (*p != close) { emsg("unterminated #include"); return; }
  }

  if (resolve_include(name, quoted, path, sizeof(path)) < 0)
  { emsg2("cannot find include: ", name); return; }

  if (include_depth >= MAX_INCLUDE_DEPTH)
  { emsg("include depth exceeded"); return; }

  txt = load_file(path, NULL);
  if (!txt) { emsg2("cannot read: ", path); return; }

  path_dirname(path, dir, sizeof(dir));
  quote_dir_stack[include_depth] = pool_strdup(dir);
  include_depth++;

  saved_filename = cur_filename;
  saved_line = cur_line;

  process_text(txt, path);

  include_depth--;
  cur_filename = saved_filename;
  cur_line = saved_line;
  /* txt buffer is leaked into the heap; on host build that's fine. */
}

/*===========================================================================*/
/*  Main per-file processor                                                  */
/*===========================================================================*/

static void process_text(const char *text, const char *filename)
{
  char line[LINE_BUF_BYTES];
  char expanded[EXPAND_BUF_BYTES];
  const char *src = text;
  int lines_consumed;
  int in_block = 0;       /* tracks block comments across lines */

  cur_filename = filename;
  cur_line = 1;

  while ((src = read_logical_line(src, line, sizeof(line), &lines_consumed)) != NULL)
  {
    const char *p;
    /* Strip comments first; this respects in_block across logical lines. */
    strip_comments(line, &in_block);
    p = skip_hws(line);

    /* Determine if this is a directive (need to handle even when skipping
     * for #else/#endif). */
    if (*p == '#')
    {
      const char *d;
      int dlen;
      p = skip_hws(p + 1);
      d = p;
      while (is_id_cont((unsigned char)*p)) p++;
      dlen = (int)(p - d);

      /* Comments removed inside directive lines. */
      {
        char rest[LINE_BUF_BYTES];
        int dummy = 0;
        int rl = (int)strlen(p);
        if (rl >= (int)sizeof(rest)) rl = sizeof(rest) - 1;
        memcpy(rest, p, (size_t)rl);
        rest[rl] = '\0';
        strip_comments(rest, &dummy);

        if (dlen == 5 && memcmp(d, "ifdef", 5) == 0)
        {
          handle_ifdef(rest, 0);
        }
        else if (dlen == 6 && memcmp(d, "ifndef", 6) == 0)
        {
          handle_ifdef(rest, 1);
        }
        else if (dlen == 4 && memcmp(d, "else", 4) == 0)
        {
          handle_else();
        }
        else if (dlen == 5 && memcmp(d, "endif", 5) == 0)
        {
          handle_endif();
        }
        else if (branch_active())
        {
          if (dlen == 6 && memcmp(d, "define", 6) == 0)
            handle_define(rest);
          else if (dlen == 7 && memcmp(d, "include", 7) == 0)
            handle_include(rest);
          else if (dlen == 0)
            ; /* null directive */
          else
          {
            char ds[64];
            int n = dlen < 60 ? dlen : 60;
            memcpy(ds, d, (size_t)n);
            ds[n] = '\0';
            emsg2("unsupported directive: #", ds);
          }
        }
      }

      /* Emit a blank line so error reporting in downstream tools stays
       * approximately right. */
      out_write("\n", 1);
      cur_line += lines_consumed;
      continue;
    }

    if (!branch_active())
    {
      out_write("\n", 1);
      cur_line += lines_consumed;
      continue;
    }

    /* expand macros (comments already stripped above) */
    {
      int rc = expand_pass(line, expanded, sizeof(expanded), NULL);
      const char *to_emit = expanded;
      int iter = 0;
      char buf2[EXPAND_BUF_BYTES];
      const char *cur = expanded;
      char *other = buf2;
      while (rc > 0 && iter < 50)
      {
        rc = expand_pass(cur, other, sizeof(buf2), NULL);
        if (rc < 0) break;
        {
          const char *t = cur;
          cur = other;
          /* swap by reusing expanded as the other buffer */
          other = (char *)t;
        }
        iter++;
      }
      to_emit = cur;
      out_str(to_emit);
    }
    out_write("\n", 1);
    cur_line += lines_consumed;
  }
}

/*===========================================================================*/
/*  Top-level                                                                */
/*===========================================================================*/

static void usage(void)
{
  IO_PRINT_ERR(
    "usage: cpp [-D NAME[=VAL]] [-I PATH] [-o output] input.c\n");
}

#ifdef CPP_HOST
int main(int argc, char **argv)
{
  int i;
  const char *input_path = NULL;
  const char *output_path = NULL;

  /* allocate string pool */
  str_pool_size = STR_POOL_BYTES;
  str_pool = (char *)malloc(STR_POOL_BYTES);
  if (!str_pool) { fprintf(stderr, "cpp: out of memory\n"); return 1; }

  for (i = 1; i < argc; i++)
  {
    if (strcmp(argv[i], "-D") == 0 && i + 1 < argc)
    {
      const char *a = argv[++i];
      const char *eq = strchr(a, '=');
      if (eq)
      {
        char nm[ID_MAX_LEN];
        int n = (int)(eq - a);
        if (n >= ID_MAX_LEN) n = ID_MAX_LEN - 1;
        memcpy(nm, a, (size_t)n); nm[n] = '\0';
        macro_define(a, n, eq + 1, -1, NULL);
      }
      else
      {
        macro_define(a, (int)strlen(a), "1", -1, NULL);
      }
    }
    else if (strncmp(argv[i], "-D", 2) == 0)
    {
      const char *a = argv[i] + 2;
      const char *eq = strchr(a, '=');
      if (eq)
        macro_define(a, (int)(eq - a), eq + 1, -1, NULL);
      else
        macro_define(a, (int)strlen(a), "1", -1, NULL);
    }
    else if (strcmp(argv[i], "-I") == 0 && i + 1 < argc)
    {
      if (include_dir_count < MAX_INCLUDE_DIRS)
        include_dirs[include_dir_count++] = pool_strdup(argv[++i]);
    }
    else if (strncmp(argv[i], "-I", 2) == 0)
    {
      if (include_dir_count < MAX_INCLUDE_DIRS)
        include_dirs[include_dir_count++] = pool_strdup(argv[i] + 2);
    }
    else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc)
    {
      output_path = argv[++i];
    }
    else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
    {
      usage();
      return 0;
    }
    else if (argv[i][0] == '-')
    {
      fprintf(stderr, "cpp: unknown option %s\n", argv[i]);
      return 1;
    }
    else
    {
      input_path = argv[i];
    }
  }

  if (!input_path) { usage(); return 1; }

  if (output_path)
  {
    out_fp = fopen(output_path, "wb");
    if (!out_fp) { fprintf(stderr, "cpp: cannot open %s\n", output_path); return 1; }
  }

  {
    char *txt = load_file(input_path, NULL);
    char dir[256];
    if (!txt) { fprintf(stderr, "cpp: cannot read %s\n", input_path); return 1; }
    path_dirname(input_path, dir, sizeof(dir));
    quote_dir_stack[include_depth++] = pool_strdup(dir);
    process_text(txt, input_path);
    include_depth--;
  }

  out_flush();
  if (out_fp) fclose(out_fp);

  return has_error ? 1 : 0;
}
#else
int main()
{
  /* BDOS entry: parse argv via sys_shell_argc/argv, mirror host behavior. */
  int argc = sys_shell_argc();
  char **argv = sys_shell_argv();
  int i;
  const char *input_path = NULL;
  const char *output_path = NULL;

  str_pool_size = STR_POOL_BYTES;
  str_pool = (char *)sys_heap_alloc(STR_POOL_BYTES);
  if (!str_pool) { sys_print_str("cpp: out of memory\n"); return 1; }

  for (i = 1; i < argc; i++)
  {
    if (strcmp(argv[i], "-D") == 0 && i + 1 < argc)
    {
      const char *a = argv[++i];
      const char *eq = strchr(a, '=');
      if (eq) macro_define(a, (int)(eq - a), eq + 1, -1, NULL);
      else    macro_define(a, (int)strlen(a), "1", -1, NULL);
    }
    else if (strcmp(argv[i], "-I") == 0 && i + 1 < argc)
    {
      if (include_dir_count < MAX_INCLUDE_DIRS)
        include_dirs[include_dir_count++] = pool_strdup(argv[++i]);
    }
    else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc)
    {
      output_path = argv[++i];
    }
    else if (argv[i][0] != '-')
    {
      input_path = argv[i];
    }
  }
  if (!input_path) { usage(); return 1; }

  if (output_path)
  {
    sys_fs_delete(output_path);
    sys_fs_create(output_path);
    out_fd = sys_fs_open(output_path);
    if (out_fd < 0) { sys_print_str("cpp: cannot open output\n"); return 1; }
  }
  else
  {
    /* No output file: write to stdout — BDOS shell captures via pipe. */
    out_fd = 1;
  }

  {
    char *txt = load_file(input_path, NULL);
    char dir[256];
    if (!txt) { sys_print_str("cpp: cannot read input\n"); return 1; }
    path_dirname(input_path, dir, sizeof(dir));
    quote_dir_stack[include_depth++] = pool_strdup(dir);
    process_text(txt, input_path);
    include_depth--;
  }

  out_flush();
  if (out_fd > 0) sys_fs_close(out_fd);
  return has_error ? 1 : 0;
}
#endif
