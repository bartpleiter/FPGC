/*****************************************************************************/
/*                                                                           */
/*                  asm-link  —  B32P3 Assembler + Linker                    */
/*                                                                           */
/*  Native assembler/linker for the modern toolchain (cproc + QBE) running   */
/*  as a userBDOS program. Replaces ASMPY for on-device self-hosting.        */
/*                                                                           */
/*  - Always emits userBDOS binaries: header (jump Main, nop, .dw filesize), */
/*    relocation table appended after program data.                          */
/*  - Always uses the relocation table mechanism (not the legacy savpc PIC). */
/*  - Accepts multiple input .asm files and links them together (renaming   */
/*    .L local labels and conflicting non-global symbols).                  */
/*  - Supports the ELF-style data directives emitted by QBE:                */
/*       .ascii, .byte, .short, .int (numeric or SYMBOL+OFFSET),            */
/*       .fill N,S,V, .balign N, .globl/.global NAME, .text                 */
/*  - Supports the byte-addressable memory instructions:                    */
/*       readb / readbu / readh / readhu / writeb / writeh                  */
/*  - Writes raw 32-bit words directly (NOT the ASMPY .list text format).   */
/*                                                                           */
/*  Output binary layout (matches ASMPY --header --independent):            */
/*       word 0 : jump Main      (relocatable, type 2)                      */
/*       word 1 : nop                                                        */
/*       word 2 : program_size_in_words (used by BDOS loader)               */
/*       word 3..N-1 : program code/data                                    */
/*       word N   : reloc_count                                              */
/*       word N+1..: reloc entries (byte_offset<<8) | type                  */
/*                                                                           */
/*****************************************************************************/

/* Build target: by default we compile for BDOS via cproc/QBE.
 * Defining ASMLINK_HOST builds a host (gcc/Linux) variant for testing,
 * using stdio for file I/O and printing to stderr.
 */

#ifdef ASMLINK_HOST
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
/*  I/O abstraction                                                          */
/*===========================================================================*/

#ifdef ASMLINK_HOST

/* Host: a simple table maps integer fd to FILE*. We fake the BDOS API
 * using stdio.
 */
#define MAX_HOST_FDS 16
static FILE *host_files[MAX_HOST_FDS];

static int host_open(const char *path)
{
  int i;
  FILE *f;

  /* Try read+write update first; if not exists, fall back to write+read. */
  f = fopen(path, "rb+");
  if (!f) f = fopen(path, "rb");
  if (!f) return -1;

  for (i = 0; i < MAX_HOST_FDS; i++)
  {
    if (host_files[i] == NULL)
    {
      host_files[i] = f;
      return i;
    }
  }
  fclose(f);
  return -1;
}

static int host_create(const char *path)
{
  FILE *f = fopen(path, "wb");
  if (!f) return -1;
  fclose(f);
  return 0;
}

static int host_delete(const char *path)
{
  return remove(path);
}

static int host_close(int fd)
{
  if (fd < 0 || fd >= MAX_HOST_FDS || host_files[fd] == NULL) return -1;
  fclose(host_files[fd]);
  host_files[fd] = NULL;
  return 0;
}

static int host_filesize_words(int fd)
{
  long pos;
  long end;
  if (fd < 0 || fd >= MAX_HOST_FDS || host_files[fd] == NULL) return -1;
  pos = ftell(host_files[fd]);
  fseek(host_files[fd], 0, SEEK_END);
  end = ftell(host_files[fd]);
  fseek(host_files[fd], pos, SEEK_SET);
  /* round up to whole words for word-count semantics */
  return (int)((end + 3) / 4);
}

static int host_read_words(int fd, void *buf, int count_words)
{
  size_t got;
  if (fd < 0 || fd >= MAX_HOST_FDS || host_files[fd] == NULL) return -1;
  got = fread(buf, 4, (size_t)count_words, host_files[fd]);
  return (int)got;
}

static int host_write_words(int fd, const void *buf, int count_words)
{
  size_t put;
  if (fd < 0 || fd >= MAX_HOST_FDS || host_files[fd] == NULL) return -1;
  put = fwrite(buf, 4, (size_t)count_words, host_files[fd]);
  return (int)put;
}

#define IO_OPEN(p)            host_open(p)
#define IO_CREATE(p)          host_create(p)
#define IO_DELETE(p)          host_delete(p)
#define IO_CLOSE(fd)          host_close(fd)
#define IO_FILESIZE_WORDS(fd) host_filesize_words(fd)
#define IO_READ_WORDS(fd, b, n)  host_read_words(fd, b, n)
#define IO_WRITE_WORDS(fd, b, n) host_write_words(fd, b, n)
#define IO_PRINT(s)           fputs(s, stderr)
#define IO_HEAP_ALLOC(n)      malloc((size_t)(n))
#define IO_ARGC()             host_argc
#define IO_ARGV()             host_argv
#define IO_GETCWD()           "."

static int    host_argc;
static char **host_argv;

#else /* BDOS build */

#define IO_OPEN(p)            sys_fs_open(p)
#define IO_CREATE(p)          sys_fs_create(p)
#define IO_DELETE(p)          sys_fs_delete(p)
#define IO_CLOSE(fd)          sys_fs_close(fd)
#define IO_FILESIZE_WORDS(fd) sys_fs_filesize(fd)
#define IO_READ_WORDS(fd, b, n)  sys_fs_read(fd, b, n)
#define IO_WRITE_WORDS(fd, b, n) sys_fs_write(fd, b, n)
#define IO_PRINT(s)           sys_putstr(s)
#define IO_HEAP_ALLOC(n)      sys_heap_alloc(n)
#define IO_ARGC()             sys_shell_argc()
#define IO_ARGV()             sys_shell_argv()
#define IO_GETCWD()           sys_shell_getcwd()

#endif

/*===========================================================================*/
/*  Limits and buffer sizes                                                  */
/*===========================================================================*/

#define MAX_FILES         32
#define MAX_LINE_LEN      512
#define MAX_TOKENS        16
#define MAX_PATH          256

#define MAX_LINES         (256 * 1024)   /* total parsed lines across all files */
#define LABEL_NAME_LEN    64
#define MAX_LABELS        16384
#define MAX_RELOCS        16384

#define STR_POOL_BYTES    (2 * 1024 * 1024)  /* 2 MiB for interned strings */
#define BYTE_POOL_BYTES   (512 * 1024)       /* 512 KiB for ELF byte data */
#define OUTPUT_WORDS      (256 * 1024)       /* 1 MiB output */

/*===========================================================================*/
/*  Instruction encoding constants                                           */
/*===========================================================================*/

#define OP_ARITH    0x0u
#define OP_ARITHC   0x1u
#define OP_ARITHMC  0x2u
#define OP_ARITHM   0x3u
#define OP_RETI     0x4u
#define OP_SAVPC    0x5u
#define OP_BRANCH   0x6u
#define OP_CCACHE   0x7u
#define OP_JUMPR    0x8u
#define OP_JUMP     0x9u
#define OP_POP      0xAu
#define OP_PUSH     0xBu
#define OP_INTID    0xCu
#define OP_WRITE    0xDu
#define OP_READ     0xEu
#define OP_HALT     0xFu

#define ARITH_OR      0x0u
#define ARITH_AND     0x1u
#define ARITH_XOR     0x2u
#define ARITH_ADD     0x3u
#define ARITH_SUB     0x4u
#define ARITH_SHIFTL  0x5u
#define ARITH_SHIFTR  0x6u
#define ARITH_NOT     0x7u
#define ARITH_SLT     0xAu
#define ARITH_SLTU    0xBu
#define ARITH_LOAD    0xCu
#define ARITH_LOADHI  0xDu
#define ARITH_SHIFTRS 0xEu

/* Multi-cycle integer op-codes */
#define ARITHM_MULTS   0x0u
#define ARITHM_MULTU   0x1u
#define ARITHM_MULTFP  0x2u
#define ARITHM_DIVS    0x3u
#define ARITHM_DIVU    0x4u
#define ARITHM_DIVFP   0x5u
#define ARITHM_MODS    0x6u
#define ARITHM_MODU    0x7u
/* FP64 coprocessor op-codes */
#define ARITHM_FMUL    0x8u
#define ARITHM_FADD    0x9u
#define ARITHM_FSUB    0xAu
#define ARITHM_FLD     0xBu
#define ARITHM_FSTHI   0xCu
#define ARITHM_FSTLO   0xDu
#define ARITHM_MULSHI  0xEu
#define ARITHM_MULTUHI 0xFu

#define BR_BEQ  0x0u
#define BR_BGT  0x1u
#define BR_BGE  0x2u
#define BR_BNE  0x4u
#define BR_BLT  0x5u
#define BR_BLE  0x6u

/* Memory sub-opcodes (encoded in different positions for read/write) */
#define MEM_WORD     0x0u
#define MEM_BYTE     0x1u
#define MEM_HALF     0x2u
#define MEM_BYTE_U   0x5u
#define MEM_HALF_U   0x6u

/* Relocation entry types */
#define RELOC_DATA_WORD 0u
#define RELOC_LOAD_PAIR 1u
#define RELOC_JUMP      2u

/* Sections (preserved order) */
#define SEC_CODE   0
#define SEC_DATA   1
#define SEC_RDATA  2
#define SEC_BSS    3
#define NUM_SECTIONS 4

/*===========================================================================*/
/*  Line records                                                             */
/*===========================================================================*/

/* line.kind values */
enum {
  LK_INSTR = 1,    /* general instruction (text holds full line) */
  LK_LABEL,        /* label definition;  text = label name (no colon) */
  LK_DW_NUM,       /* one 32-bit data word, immediate value */
  LK_DW_LABREF,    /* one 32-bit data word, value = label_addr + offset */
  LK_BYTES,        /* runs of ELF bytes accumulated in byte_pool */
  LK_INT_LABREF,   /* boundary inside ELF stream — .int LABEL+OFFSET */
  LK_GLOBAL,       /* .globl/.global NAME (metadata only, removed) */
  LK_DIR,          /* section directive .text/.data/.rdata/.bss */
  LK_BALIGN,       /* .balign N (alignment within current section) */
  LK_LOAD32,       /* pseudo: load32 N reg */
  LK_ADDR2REG,     /* pseudo: addr2reg LABEL reg (becomes load+loadhi+RELOC) */
  LK_LOAD_LABEL,   /* expansion of addr2reg: low half (records reloc) */
  LK_LOADHI_LABEL  /* expansion of addr2reg: high half */
};

/* Parallel arrays for lines */
static int   *line_kind;
static char **line_text;     /* pointer into str_pool (or NULL) */
static int   *line_section;  /* SEC_* */
static int   *line_file_idx; /* which input file */

/* For LK_DW_NUM: integer value */
static unsigned int *line_value;

/* For LK_DW_LABREF / LK_INT_LABREF / LK_LOAD_LABEL / LK_LOADHI_LABEL /
 * LK_ADDR2REG / branch+jump+load with label arg: target label name and offset */
static char **line_label;    /* label name (in str_pool) or NULL */
static int   *line_label_off;/* signed offset added to label */

/* For LK_BYTES: offset into byte_pool and length */
static int   *line_byte_off;
static int   *line_byte_len;

/* For LK_BALIGN: alignment value */
static int   *line_align;

/* For LK_DIR: which section directive */
static int   *line_dir_sec;

/* Computed byte address after layout (set in pass_layout) */
static int   *line_addr;

/* Special flags:
 *   1 = header instruction (jump Main ought to be RELOC_JUMP rather than
 *       being rewritten to jumpo). Set by add_header().
 *   2 = "first half of an addr2reg pair" — emit RELOC_LOAD_PAIR for it.
 */
#define FLAG_HEADER     0x1
#define FLAG_LOAD_PAIR  0x2
static int   *line_flags;

static int   line_count;

/*===========================================================================*/
/*  Label table                                                              */
/*===========================================================================*/

static char (*label_names)[LABEL_NAME_LEN];
static int   *label_addr;   /* byte address */
static int    label_count;

/* Per-file global-symbol bookkeeping (used during link-time renaming) */
/* For each label: which file_idx defined it, and whether it's marked global */
static int   *label_file;
static int   *label_global;

/*===========================================================================*/
/*  String + byte pools                                                      */
/*===========================================================================*/

static char *str_pool;
static int   str_pool_pos;
static int   str_pool_size;

static unsigned char *byte_pool;
static int   byte_pool_pos;
static int   byte_pool_size;

/*===========================================================================*/
/*  Output                                                                   */
/*===========================================================================*/

static unsigned int *output_words;
static int    output_count;

static unsigned int *reloc_entries;  /* packed (offset<<8)|type */
static int    reloc_count;

/*===========================================================================*/
/*  Globals: input files                                                     */
/*===========================================================================*/

static char *file_paths[MAX_FILES];
static char *file_prefix[MAX_FILES];   /* basename used for label renaming */
static int   num_files;

static char *output_path;
static int   verbose;
static int   has_error;
static int   dump_labels;

/*===========================================================================*/
/*  Utilities                                                                */
/*===========================================================================*/

static void emsg(const char *s)
{
  IO_PRINT("asm-link error: ");
  IO_PRINT((char *)s);
  IO_PRINT("\n");
  has_error = 1;
}

static void emsg2(const char *s, const char *t)
{
  IO_PRINT("asm-link error: ");
  IO_PRINT((char *)s);
  IO_PRINT((char *)t);
  IO_PRINT("\n");
  has_error = 1;
}

static void vmsg(const char *s)
{
  if (verbose) IO_PRINT((char *)s);
}

static void vmsg_int(int n)
{
  char buf[16];
  if (!verbose) return;
  /* simple itoa for verbose printing */
  {
    int i = 0;
    int neg = 0;
    int j;
    char tmp[16];
    if (n < 0) { neg = 1; n = -n; }
    if (n == 0) tmp[i++] = '0';
    while (n > 0) { tmp[i++] = '0' + (n % 10); n /= 10; }
    if (neg) tmp[i++] = '-';
    for (j = 0; j < i; j++) buf[j] = tmp[i - 1 - j];
    buf[i] = 0;
  }
  IO_PRINT(buf);
}

static char *pool_strdup_n(const char *s, int n)
{
  char *r;
  int i;
  if (str_pool_pos + n + 1 > str_pool_size)
  {
    emsg("string pool exhausted");
    return NULL;
  }
  r = &str_pool[str_pool_pos];
  for (i = 0; i < n; i++) r[i] = s[i];
  r[n] = 0;
  str_pool_pos += n + 1;
  return r;
}

static char *pool_strdup(const char *s)
{
  return pool_strdup_n(s, (int)strlen(s));
}

static int byte_pool_emit(unsigned char b)
{
  if (byte_pool_pos >= byte_pool_size)
  {
    emsg("byte pool exhausted");
    return -1;
  }
  byte_pool[byte_pool_pos++] = b;
  return 0;
}

/* Allocate a new line slot and zero its fields. */
static int new_line(int kind)
{
  int i;
  if (line_count >= MAX_LINES)
  {
    emsg("too many lines");
    return -1;
  }
  i = line_count++;
  line_kind[i]      = kind;
  line_text[i]      = NULL;
  line_section[i]   = SEC_CODE;
  line_file_idx[i]  = 0;
  line_value[i]     = 0;
  line_label[i]     = NULL;
  line_label_off[i] = 0;
  line_byte_off[i]  = 0;
  line_byte_len[i]  = 0;
  line_align[i]     = 0;
  line_dir_sec[i]   = SEC_CODE;
  line_addr[i]      = 0;
  line_flags[i]     = 0;
  return i;
}

/*===========================================================================*/
/*  String helpers                                                           */
/*===========================================================================*/

static int is_id_start(int c)
{
  return (c == '_' || c == '.' || (c >= 'a' && c <= 'z') ||
          (c >= 'A' && c <= 'Z'));
}

static int is_id_cont(int c)
{
  return (c == '_' || c == '.' || (c >= 'a' && c <= 'z') ||
          (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'));
}

static int parse_register(const char *s)
{
  int v;
  if (s[0] != 'r') return -1;
  if (s[1] < '0' || s[1] > '9') return -1;
  if (s[2] == 0) return s[1] - '0';
  if (s[2] >= '0' && s[2] <= '9' && s[3] == 0)
  {
    v = (s[1] - '0') * 10 + (s[2] - '0');
    if (v >= 0 && v <= 15) return v;
  }
  return -1;
}

static int parse_number(const char *s, int *out)
{
  int neg = 0;
  int i = 0;
  unsigned int v = 0;

  if (s[0] == '-') { neg = 1; i = 1; }
  else if (s[0] == '+') { i = 1; }

  if (s[i] == 0) return 0;

  if (s[i] == '0' && (s[i + 1] == 'x' || s[i + 1] == 'X'))
  {
    i += 2;
    if (s[i] == 0) return 0;
    while (s[i])
    {
      v <<= 4;
      if (s[i] >= '0' && s[i] <= '9') v |= (unsigned)(s[i] - '0');
      else if (s[i] >= 'a' && s[i] <= 'f') v |= (unsigned)(s[i] - 'a' + 10);
      else if (s[i] >= 'A' && s[i] <= 'F') v |= (unsigned)(s[i] - 'A' + 10);
      else return 0;
      i++;
    }
  }
  else if (s[i] == '0' && (s[i + 1] == 'b' || s[i + 1] == 'B'))
  {
    i += 2;
    if (s[i] == 0) return 0;
    while (s[i])
    {
      v <<= 1;
      if (s[i] == '1') v |= 1;
      else if (s[i] != '0') return 0;
      i++;
    }
  }
  else
  {
    while (s[i])
    {
      if (s[i] < '0' || s[i] > '9') return 0;
      v = v * 10 + (unsigned)(s[i] - '0');
      i++;
    }
  }

  if (neg) *out = -(int)v;
  else     *out =  (int)v;
  return 1;
}

/* In-place tokenize by whitespace. Returns count. */
static int tokenize(char *s, char **toks, int max_toks)
{
  int n = 0;
  int in_tok = 0;
  while (*s)
  {
    if (*s == ' ' || *s == '\t')
    {
      if (in_tok) { *s = 0; in_tok = 0; }
    }
    else
    {
      if (!in_tok)
      {
        if (n >= max_toks) return n;
        toks[n++] = s;
        in_tok = 1;
      }
    }
    s++;
  }
  return n;
}

/* Same, but treats commas as separators too. Used for .fill N,S,V */
static int tokenize_csv(char *s, char **toks, int max_toks)
{
  int n = 0;
  int in_tok = 0;
  while (*s)
  {
    if (*s == ' ' || *s == '\t' || *s == ',')
    {
      if (in_tok) { *s = 0; in_tok = 0; }
    }
    else
    {
      if (!in_tok)
      {
        if (n >= max_toks) return n;
        toks[n++] = s;
        in_tok = 1;
      }
    }
    s++;
  }
  return n;
}

/* Strip leading whitespace */
static char *lstrip(char *s)
{
  while (*s == ' ' || *s == '\t') s++;
  return s;
}

/* Locate a label in label_names[]. Returns index or -1. */
static int find_label(const char *name)
{
  int i;
  for (i = 0; i < label_count; i++)
  {
    if (strcmp(label_names[i], name) == 0) return i;
  }
  return -1;
}

static int add_label(const char *name, int file_idx)
{
  int n;
  if (label_count >= MAX_LABELS)
  {
    emsg("too many labels");
    return -1;
  }
  n = (int)strlen(name);
  if (n >= LABEL_NAME_LEN)
  {
    emsg2("label name too long: ", name);
    return -1;
  }
  if (find_label(name) >= 0)
  {
    emsg2("duplicate label: ", name);
    return -1;
  }
  {
    int i;
    for (i = 0; i < n; i++) label_names[label_count][i] = name[i];
    label_names[label_count][n] = 0;
  }
  label_addr[label_count] = -1;
  label_file[label_count] = file_idx;
  label_global[label_count] = 0;
  label_count++;
  return label_count - 1;
}

/* Return basename of path without extension, into a pool string. */
static char *make_prefix(const char *path)
{
  const char *p = path;
  const char *base = path;
  const char *dot;
  int n;
  char tmp[64];
  int i;

  while (*p) { if (*p == '/') base = p + 1; p++; }
  dot = base;
  while (*dot && *dot != '.') dot++;
  n = (int)(dot - base);
  if (n >= (int)sizeof(tmp)) n = (int)sizeof(tmp) - 1;
  for (i = 0; i < n; i++)
  {
    char c = base[i];
    if (c == '-' || c == '.') c = '_';
    tmp[i] = c;
  }
  tmp[n] = 0;
  return pool_strdup(tmp);
}

/*===========================================================================*/
/*  File reading                                                             */
/*===========================================================================*/

/* Read the entire file at `path` into a freshly allocated buffer (heap) and
 * return it as a NUL-terminated string. The buffer is sized to byte length;
 * filesize is in words on BDOS, multiplied by 4. We trim trailing NULs so the
 * source text doesn't contain spurious bytes.
 */
static char *read_file(const char *path, int *out_bytes)
{
  int fd;
  int size_w;
  int size_b;
  char *buf;
  int total;
  int chunk;
  int got;

  fd = IO_OPEN((char *)path);
  if (fd < 0)
  {
    emsg2("cannot open file: ", path);
    return NULL;
  }
  size_w = IO_FILESIZE_WORDS(fd);
  if (size_w < 0)
  {
    IO_CLOSE(fd);
    emsg2("invalid file: ", path);
    return NULL;
  }
  size_b = size_w * 4;

  buf = (char *)IO_HEAP_ALLOC(size_b + 4);
  if (!buf)
  {
    IO_CLOSE(fd);
    emsg("out of memory reading file");
    return NULL;
  }

  total = 0;
  while (total < size_w)
  {
    chunk = size_w - total;
    if (chunk > 256) chunk = 256;
    got = IO_READ_WORDS(fd, (unsigned int *)(buf + total * 4), chunk);
    if (got <= 0) break;
    total += got;
  }
  IO_CLOSE(fd);

  /* Trim trailing zero padding (BRFS pads files to whole words with NUL). */
  while (size_b > 0 && buf[size_b - 1] == 0) size_b--;
  buf[size_b] = 0;
  if (out_bytes) *out_bytes = size_b;
  return buf;
}

/*===========================================================================*/
/*  Pre-parse: split a file's text into lines, strip comments, return into   */
/*  the line array. Per-file pass; does .L renaming inline.                  */
/*===========================================================================*/

/* Return 1 if this token is an identifier-class string starting with '.L' */
static int is_local_label_token(const char *s, int len)
{
  if (len < 2) return 0;
  if (s[0] != '.' || s[1] != 'L') return 0;
  /* The .L prefix on its own (e.g. ".L"), or with id-cont chars after */
  return 1;
}

/* Rewrite a (mutable) line buffer:
 *   replace every occurrence of `.L<rest>` (where `.L<rest>` is bounded by
 *   non-id-cont characters or string boundary) with `.L_<prefix>_<rest>`.
 * Output is written into `out` (size out_cap). Returns 1 on success.
 *
 * We do NOT rewrite occurrences inside double-quoted strings.
 */
static int rewrite_local_labels(const char *in, char *out, int out_cap,
                                const char *prefix)
{
  int i = 0;
  int o = 0;
  int in_quote = 0;
  int prefix_len = (int)strlen(prefix);

  while (in[i])
  {
    char c = in[i];
    if (c == '"')
    {
      /* Toggle on unescaped quote */
      if (i == 0 || in[i - 1] != '\\') in_quote = !in_quote;
      if (o + 1 >= out_cap) return 0;
      out[o++] = c;
      i++;
      continue;
    }
    if (in_quote)
    {
      if (o + 1 >= out_cap) return 0;
      out[o++] = c;
      i++;
      continue;
    }

    /* Check for .L identifier start, with no preceding id-cont char */
    if (c == '.' && in[i + 1] == 'L'
        && (i == 0 || !is_id_cont((unsigned char)in[i - 1])))
    {
      /* Look at character after the L. If it's id-cont OR end-of-id (colon, etc.),
       * we treat this as a .L label. We rewrite to .L_<prefix>_<rest>.
       * "rest" is everything after ".L" up to the first non-id-cont char.
       */
      int j = i + 2;
      int rest_start = j;
      int rest_len;
      while (in[j] && is_id_cont((unsigned char)in[j])) j++;
      rest_len = j - rest_start;
      /* We always rewrite .L tokens (even bare ".L" with no rest). */
      if (o + 3 + prefix_len + 1 + rest_len >= out_cap) return 0;
      out[o++] = '.';
      out[o++] = 'L';
      out[o++] = '_';
      {
        int k;
        for (k = 0; k < prefix_len; k++) out[o++] = prefix[k];
      }
      out[o++] = '_';
      {
        int k;
        for (k = 0; k < rest_len; k++) out[o++] = in[rest_start + k];
      }
      i = j;
      continue;
    }

    if (o + 1 >= out_cap) return 0;
    out[o++] = c;
    i++;
  }
  out[o] = 0;
  return 1;
}

/* Rewrite a (mutable) line buffer: replace every whole-word occurrence of
 * `name` (one of the conflicting non-global labels) with `__<prefix>__<name>`.
 * We respect quoted strings.
 *
 * Used in pass_resolve_conflicts(). Operates on a single label name only; for
 * efficiency we'd batch them, but simplicity wins.
 */
static int rewrite_label_word(const char *in, char *out, int out_cap,
                              const char *name, const char *prefix)
{
  int name_len = (int)strlen(name);
  int prefix_len = (int)strlen(prefix);
  int i = 0;
  int o = 0;
  int in_quote = 0;

  while (in[i])
  {
    char c = in[i];
    if (c == '"')
    {
      if (i == 0 || in[i - 1] != '\\') in_quote = !in_quote;
      if (o + 1 >= out_cap) return 0;
      out[o++] = c;
      i++;
      continue;
    }
    if (in_quote)
    {
      if (o + 1 >= out_cap) return 0;
      out[o++] = c;
      i++;
      continue;
    }

    /* Boundary check: previous char must NOT be id-cont, and the matched
     * suffix must end at non-id-cont.
     */
    if ((i == 0 || !is_id_cont((unsigned char)in[i - 1]))
        && strncmp(&in[i], name, (size_t)name_len) == 0
        && !is_id_cont((unsigned char)in[i + name_len]))
    {
      if (o + 4 + prefix_len + name_len >= out_cap) return 0;
      out[o++] = '_';
      out[o++] = '_';
      {
        int k;
        for (k = 0; k < prefix_len; k++) out[o++] = prefix[k];
      }
      out[o++] = '_';
      out[o++] = '_';
      {
        int k;
        for (k = 0; k < name_len; k++) out[o++] = name[k];
      }
      i += name_len;
      continue;
    }

    if (o + 1 >= out_cap) return 0;
    out[o++] = c;
    i++;
  }
  out[o] = 0;
  return 1;
}

/*===========================================================================*/
/*  Pass 1: parse all files into line records (with .L renaming applied)     */
/*===========================================================================*/

/* Strip a single source line: kill semicolon-style comments and any in-line
 * C-style block comments that begin and end on the same line. Trim leading
 * and trailing whitespace. Returns length of cleaned text in `out`.
 */
static int clean_line(const char *in, char *out, int out_cap)
{
  int i;
  int o;
  int in_quote;
  int len;

  /* First, drop trailing semicolon comments. */
  i = 0; o = 0; in_quote = 0;
  while (in[i] && o + 1 < out_cap)
  {
    char c = in[i];
    if (c == '"')
    {
      if (i == 0 || in[i - 1] != '\\') in_quote = !in_quote;
      out[o++] = c;
      i++;
      continue;
    }
    if (!in_quote && c == ';') break;
    if (!in_quote && c == '/' && in[i + 1] == '*')
    {
      /* Skip until matching */
      int j = i + 2;
      while (in[j])
      {
        if (in[j] == '*' && in[j + 1] == '/') { j += 2; break; }
        j++;
      }
      i = j;
      continue;
    }
    out[o++] = c;
    i++;
  }
  out[o] = 0;

  /* Trim leading whitespace */
  {
    char *p = out;
    while (*p == ' ' || *p == '\t') p++;
    if (p != out)
    {
      int k = 0;
      while (p[k]) { out[k] = p[k]; k++; }
      out[k] = 0;
      o = k;
    }
  }
  /* Trim trailing whitespace */
  len = o;
  while (len > 0 && (out[len - 1] == ' ' || out[len - 1] == '\t' ||
                     out[len - 1] == '\r')) len--;
  out[len] = 0;
  return len;
}

/* Parse a single cleaned line (already trimmed, no comments) and append a
 * record to the global line array. Multiple records may be appended (e.g. for
 * a label-on-same-line, but QBE doesn't do that).
 *
 * Section state is tracked across calls via *cur_section.
 */
static void parse_one_line(char *line, int file_idx, int *cur_section)
{
  int len = (int)strlen(line);
  int idx;

  if (len == 0) return;
  if (line[0] == '#') return;  /* preprocessor leftover (e.g. # 1 "foo.c") */

  /* Section directives + .text alias for .code */
  if (line[0] == '.')
  {
    if (strncmp(line, ".text", 5) == 0
        && (line[5] == 0 || line[5] == ' ' || line[5] == '\t'))
    {
      *cur_section = SEC_CODE;
      idx = new_line(LK_DIR);
      if (idx < 0) return;
      line_dir_sec[idx] = SEC_CODE;
      line_section[idx] = SEC_CODE;
      line_file_idx[idx] = file_idx;
      return;
    }
    if (strncmp(line, ".code", 5) == 0
        && (line[5] == 0 || line[5] == ' ' || line[5] == '\t'))
    {
      *cur_section = SEC_CODE;
      idx = new_line(LK_DIR);
      if (idx < 0) return;
      line_dir_sec[idx] = SEC_CODE;
      line_section[idx] = SEC_CODE;
      line_file_idx[idx] = file_idx;
      return;
    }
    if (strncmp(line, ".data", 5) == 0
        && (line[5] == 0 || line[5] == ' ' || line[5] == '\t'))
    {
      *cur_section = SEC_DATA;
      idx = new_line(LK_DIR);
      if (idx < 0) return;
      line_dir_sec[idx] = SEC_DATA;
      line_section[idx] = SEC_DATA;
      line_file_idx[idx] = file_idx;
      return;
    }
    if (strncmp(line, ".rdata", 6) == 0
        && (line[6] == 0 || line[6] == ' ' || line[6] == '\t'))
    {
      *cur_section = SEC_RDATA;
      idx = new_line(LK_DIR);
      if (idx < 0) return;
      line_dir_sec[idx] = SEC_RDATA;
      line_section[idx] = SEC_RDATA;
      line_file_idx[idx] = file_idx;
      return;
    }
    if (strncmp(line, ".bss", 4) == 0
        && (line[4] == 0 || line[4] == ' ' || line[4] == '\t'))
    {
      *cur_section = SEC_BSS;
      idx = new_line(LK_DIR);
      if (idx < 0) return;
      line_dir_sec[idx] = SEC_BSS;
      line_section[idx] = SEC_BSS;
      line_file_idx[idx] = file_idx;
      return;
    }

    /* .balign N */
    if (strncmp(line, ".balign", 7) == 0 && (line[7] == ' ' || line[7] == '\t'))
    {
      int n = 0;
      char *rest = lstrip(line + 7);
      if (!parse_number(rest, &n)) { emsg2(".balign needs number: ", line); return; }
      idx = new_line(LK_BALIGN);
      if (idx < 0) return;
      line_align[idx] = n;
      line_section[idx] = *cur_section;
      line_file_idx[idx] = file_idx;
      return;
    }

    /* .globl / .global NAME */
    if ((strncmp(line, ".globl", 6) == 0 && (line[6] == ' ' || line[6] == '\t')) ||
        (strncmp(line, ".global", 7) == 0 && (line[7] == ' ' || line[7] == '\t')))
    {
      char *rest;
      if (line[3] == 'b') rest = lstrip(line + 6);
      else                rest = lstrip(line + 7);
      idx = new_line(LK_GLOBAL);
      if (idx < 0) return;
      line_text[idx] = pool_strdup(rest);
      line_section[idx] = *cur_section;
      line_file_idx[idx] = file_idx;
      return;
    }

    /* .ascii "..." */
    if (strncmp(line, ".ascii", 6) == 0 && (line[6] == ' ' || line[6] == '\t'))
    {
      char *p = line + 6;
      int start;
      int byte_off;
      int byte_len;
      while (*p && *p != '"') p++;
      if (*p != '"') { emsg2("missing quote in .ascii: ", line); return; }
      p++;
      start = byte_pool_pos;
      while (*p && *p != '"')
      {
        unsigned char c;
        if (*p == '\\' && p[1])
        {
          p++;
          if (*p == 'n')      c = 10;
          else if (*p == 'r') c = 13;
          else if (*p == 't') c = 9;
          else if (*p == '\\') c = '\\';
          else if (*p == '"') c = '"';
          else if (*p >= '0' && *p <= '7')
          {
            /* up to 3 octal digits */
            int v = 0;
            int k = 0;
            while (k < 3 && *p >= '0' && *p <= '7') { v = v * 8 + (*p - '0'); p++; k++; }
            c = (unsigned char)v;
            if (byte_pool_emit(c) < 0) return;
            continue;
          }
          else c = (unsigned char)*p;
          p++;
        }
        else
        {
          c = (unsigned char)*p;
          p++;
        }
        if (byte_pool_emit(c) < 0) return;
      }
      byte_off = start;
      byte_len = byte_pool_pos - start;
      idx = new_line(LK_BYTES);
      if (idx < 0) return;
      line_byte_off[idx] = byte_off;
      line_byte_len[idx] = byte_len;
      line_section[idx] = *cur_section;
      line_file_idx[idx] = file_idx;
      return;
    }

    /* .byte N */
    if (strncmp(line, ".byte", 5) == 0 && (line[5] == ' ' || line[5] == '\t'))
    {
      int v = 0;
      char *rest = lstrip(line + 5);
      int start = byte_pool_pos;
      if (!parse_number(rest, &v)) { emsg2(".byte needs number: ", line); return; }
      if (byte_pool_emit((unsigned char)(v & 0xFF)) < 0) return;
      idx = new_line(LK_BYTES);
      if (idx < 0) return;
      line_byte_off[idx] = start;
      line_byte_len[idx] = 1;
      line_section[idx] = *cur_section;
      line_file_idx[idx] = file_idx;
      return;
    }

    /* .short N */
    if (strncmp(line, ".short", 6) == 0 && (line[6] == ' ' || line[6] == '\t'))
    {
      int v = 0;
      char *rest = lstrip(line + 6);
      int start = byte_pool_pos;
      if (!parse_number(rest, &v)) { emsg2(".short needs number: ", line); return; }
      if (byte_pool_emit((unsigned char)(v & 0xFF)) < 0) return;
      if (byte_pool_emit((unsigned char)((v >> 8) & 0xFF)) < 0) return;
      idx = new_line(LK_BYTES);
      if (idx < 0) return;
      line_byte_off[idx] = start;
      line_byte_len[idx] = 2;
      line_section[idx] = *cur_section;
      line_file_idx[idx] = file_idx;
      return;
    }

    /* .int N (numeric) or .int LABEL[+/-OFFSET] */
    if (strncmp(line, ".int", 4) == 0 && (line[4] == ' ' || line[4] == '\t'))
    {
      char *rest = lstrip(line + 4);
      /* Decide numeric vs label */
      if (rest[0] == '-' || rest[0] == '+' ||
          (rest[0] >= '0' && rest[0] <= '9'))
      {
        int v = 0;
        int start = byte_pool_pos;
        if (!parse_number(rest, &v)) { emsg2(".int needs number: ", line); return; }
        if (byte_pool_emit((unsigned char)(v & 0xFF)) < 0) return;
        if (byte_pool_emit((unsigned char)((v >> 8) & 0xFF)) < 0) return;
        if (byte_pool_emit((unsigned char)((v >> 16) & 0xFF)) < 0) return;
        if (byte_pool_emit((unsigned char)((v >> 24) & 0xFF)) < 0) return;
        idx = new_line(LK_BYTES);
        if (idx < 0) return;
        line_byte_off[idx] = start;
        line_byte_len[idx] = 4;
        line_section[idx] = *cur_section;
        line_file_idx[idx] = file_idx;
        return;
      }
      else
      {
        /* SYMBOL[+/-OFFSET] */
        int off = 0;
        char *plus = strchr(rest, '+');
        char *minus = strchr(rest, '-');
        char namebuf[LABEL_NAME_LEN];
        int nlen;
        if (plus)
        {
          *plus = 0;
          if (!parse_number(plus + 1, &off)) off = 0;
        }
        else if (minus)
        {
          int v = 0;
          *minus = 0;
          if (!parse_number(minus + 1, &v)) v = 0;
          off = -v;
        }
        nlen = (int)strlen(rest);
        if (nlen >= LABEL_NAME_LEN) { emsg2(".int label too long: ", rest); return; }
        {
          int k;
          for (k = 0; k < nlen; k++) namebuf[k] = rest[k];
          namebuf[nlen] = 0;
        }
        idx = new_line(LK_INT_LABREF);
        if (idx < 0) return;
        line_label[idx] = pool_strdup(namebuf);
        line_label_off[idx] = off;
        line_section[idx] = *cur_section;
        line_file_idx[idx] = file_idx;
        return;
      }
    }

    /* .fill N,S,V */
    if (strncmp(line, ".fill", 5) == 0 && (line[5] == ' ' || line[5] == '\t'))
    {
      char *rest = lstrip(line + 5);
      char *toks[6];
      int nt;
      int n_, s_, v_;
      int start;
      int total;
      int k;
      nt = tokenize_csv(rest, toks, 6);
      if (nt == 1) { parse_number(toks[0], &n_); s_ = 1; v_ = 0; }
      else if (nt == 2) { parse_number(toks[0], &n_); parse_number(toks[1], &s_); v_ = 0; }
      else if (nt == 3) { parse_number(toks[0], &n_); parse_number(toks[1], &s_); parse_number(toks[2], &v_); }
      else { emsg2("bad .fill: ", line); return; }
      start = byte_pool_pos;
      total = n_ * s_;
      for (k = 0; k < total; k++)
      {
        if (byte_pool_emit((unsigned char)(v_ & 0xFF)) < 0) return;
      }
      idx = new_line(LK_BYTES);
      if (idx < 0) return;
      line_byte_off[idx] = start;
      line_byte_len[idx] = total;
      line_section[idx] = *cur_section;
      line_file_idx[idx] = file_idx;
      return;
    }

    /* .dw NUMBER (legacy: still useful for crt0 bare-metal ".dw 0" header) */
    if (strncmp(line, ".dw", 3) == 0 && (line[3] == ' ' || line[3] == '\t'))
    {
      int v = 0;
      char *rest = lstrip(line + 3);
      /* Accept either a single number (possibly label, but this is rare in QBE) */
      if (rest[0] == '-' || rest[0] == '+' ||
          (rest[0] >= '0' && rest[0] <= '9'))
      {
        if (!parse_number(rest, &v)) { emsg2(".dw needs number: ", line); return; }
        idx = new_line(LK_DW_NUM);
        if (idx < 0) return;
        line_value[idx] = (unsigned int)v;
        line_section[idx] = *cur_section;
        line_file_idx[idx] = file_idx;
        return;
      }
      /* label form */
      {
        int off = 0;
        char *plus = strchr(rest, '+');
        char *minus = strchr(rest, '-');
        if (plus) { *plus = 0; parse_number(plus + 1, &off); }
        else if (minus) { int x; *minus = 0; if (parse_number(minus + 1, &x)) off = -x; }
        idx = new_line(LK_DW_LABREF);
        if (idx < 0) return;
        line_label[idx] = pool_strdup(rest);
        line_label_off[idx] = off;
        line_section[idx] = *cur_section;
        line_file_idx[idx] = file_idx;
        return;
      }
    }

    /* Unknown directive — let the encoder error out later if it's really bad. */
    /* Fall through to instruction handling. */
  }

  /* Label: ends with ':' */
  if (len > 1 && line[len - 1] == ':')
  {
    char namebuf[LABEL_NAME_LEN];
    int n = len - 1;
    int k;
    if (n >= LABEL_NAME_LEN) { emsg2("label name too long: ", line); return; }
    for (k = 0; k < n; k++) namebuf[k] = line[k];
    namebuf[n] = 0;
    idx = new_line(LK_LABEL);
    if (idx < 0) return;
    line_text[idx] = pool_strdup(namebuf);
    line_section[idx] = *cur_section;
    line_file_idx[idx] = file_idx;
    return;
  }

  /* Otherwise: instruction. We special-case load32 and addr2reg so we can
   * track them as expandable pseudos. We also detect addr2reg-derived
   * load/loadhi pairs at expansion time.
   */
  {
    char tmp[MAX_LINE_LEN];
    char *toks[MAX_TOKENS];
    int nt;
    int k;
    {
      int li = 0;
      while (line[li] && li < (int)sizeof(tmp) - 1) { tmp[li] = line[li]; li++; }
      tmp[li] = 0;
    }
    nt = tokenize(tmp, toks, MAX_TOKENS);
    if (nt == 0) return;

    if (nt == 3 && strcmp(toks[0], "load32") == 0)
    {
      int v = 0;
      int reg;
      if (!parse_number(toks[1], &v)) { emsg2("bad load32 value: ", line); return; }
      reg = parse_register(toks[2]);
      if (reg < 0) { emsg2("bad load32 register: ", line); return; }
      idx = new_line(LK_LOAD32);
      if (idx < 0) return;
      line_value[idx] = (unsigned int)v;
      line_label_off[idx] = reg;     /* reuse field for register number */
      line_section[idx] = *cur_section;
      line_file_idx[idx] = file_idx;
      return;
    }

    if (nt == 3 && strcmp(toks[0], "addr2reg") == 0)
    {
      int reg = parse_register(toks[2]);
      char namebuf[LABEL_NAME_LEN];
      int off = 0;
      char *lbl = toks[1];
      char *plus = strchr(lbl, '+');
      char *minus = strchr(lbl, '-');
      int nl;
      if (reg < 0) { emsg2("bad addr2reg register: ", line); return; }
      if (plus) { *plus = 0; parse_number(plus + 1, &off); }
      else if (minus) { int x; *minus = 0; if (parse_number(minus + 1, &x)) off = -x; }
      nl = (int)strlen(lbl);
      if (nl >= LABEL_NAME_LEN) { emsg2("addr2reg label too long: ", lbl); return; }
      for (k = 0; k < nl; k++) namebuf[k] = lbl[k];
      namebuf[nl] = 0;
      idx = new_line(LK_ADDR2REG);
      if (idx < 0) return;
      line_label[idx] = pool_strdup(namebuf);
      line_label_off[idx] = off;
      line_value[idx] = (unsigned int)reg;
      line_section[idx] = *cur_section;
      line_file_idx[idx] = file_idx;
      return;
    }

    /* Plain instruction — encode on demand from text. */
    idx = new_line(LK_INSTR);
    if (idx < 0) return;
    line_text[idx] = pool_strdup(line);
    line_section[idx] = *cur_section;
    line_file_idx[idx] = file_idx;
  }
}

/* Read and parse one file into the global line array (with .L renaming). */
static int parse_file(int file_idx)
{
  char *src;
  int  src_bytes;
  int  i;
  int  cur_section = SEC_CODE;
  char raw[MAX_LINE_LEN];
  char rewritten[MAX_LINE_LEN];
  char clean[MAX_LINE_LEN];

  src = read_file(file_paths[file_idx], &src_bytes);
  if (!src) return -1;

  i = 0;
  while (i < src_bytes)
  {
    int j = 0;
    /* Read a line into raw[] */
    while (i < src_bytes && src[i] != '\n' && j < (int)sizeof(raw) - 1)
    {
      raw[j++] = src[i++];
    }
    raw[j] = 0;
    if (i < src_bytes && src[i] == '\n') i++;

    /* Rewrite .L labels with file prefix */
    if (!rewrite_local_labels(raw, rewritten, sizeof(rewritten),
                              file_prefix[file_idx]))
    {
      emsg("line too long after .L rewrite");
      return -1;
    }

    /* Strip comments + whitespace */
    if (clean_line(rewritten, clean, sizeof(clean)) == 0) continue;

    parse_one_line(clean, file_idx, &cur_section);
    if (has_error) return -1;
  }

  return 0;
}

/*===========================================================================*/
/*  Pass 2: collect labels (definitions + globals), detect cross-file        */
/*  conflicts in non-global labels, and rewrite them in their owning file.   */
/*===========================================================================*/

/* First sub-pass: walk LK_GLOBAL records and remember the names.
 * We can't add the labels to the table yet (we want to see all definitions
 * first), but we record one bit per name for later look-up.
 *
 * Strategy: for each LK_GLOBAL line, store the name in a side table; later,
 * when we add labels in pass_collect_label_defs(), we look it up to set
 * label_global[].
 */

#define MAX_GLOBALS 4096
static char *global_names[MAX_GLOBALS];
static int   global_count;

static int add_global_name(const char *name)
{
  int i;
  for (i = 0; i < global_count; i++)
  {
    if (strcmp(global_names[i], name) == 0) return 0;
  }
  if (global_count >= MAX_GLOBALS)
  {
    emsg("too many global symbols");
    return -1;
  }
  global_names[global_count++] = (char *)name;  /* already pool-interned */
  return 0;
}

static int is_global_name(const char *name)
{
  int i;
  for (i = 0; i < global_count; i++)
  {
    if (strcmp(global_names[i], name) == 0) return 1;
  }
  return 0;
}

static void pass_collect_globals(void)
{
  int i;
  for (i = 0; i < line_count; i++)
  {
    if (line_kind[i] == LK_GLOBAL)
    {
      add_global_name(line_text[i]);
    }
  }
}

/* For conflict detection: tally label definitions per name; if a non-global
 * label is defined in more than one file, rewrite it in each owning file
 * with that file's prefix.
 *
 * Implementation: bucket labels by name across all LK_LABEL lines. We use
 * the existing label table machinery as a side store: temporarily build a
 * "first definition file" map, then on second occurrence flag a conflict.
 *
 * For simplicity and correctness, we do this O(L * F) where L = number of
 * label defs and F = number of conflicting names; this is fine for our
 * scale.
 */

#define MAX_DEFNAMES (MAX_LABELS)
static char *def_names[MAX_DEFNAMES];
static int   def_files[MAX_DEFNAMES];
static int   def_count;
static char *conflict_names[MAX_DEFNAMES];
static int   conflict_count;

static int find_def(const char *name)
{
  int i;
  for (i = 0; i < def_count; i++)
  {
    if (strcmp(def_names[i], name) == 0) return i;
  }
  return -1;
}

static int is_conflict(const char *name)
{
  int i;
  for (i = 0; i < conflict_count; i++)
  {
    if (strcmp(conflict_names[i], name) == 0) return 1;
  }
  return 0;
}

static void pass_find_conflicts(void)
{
  int i;
  for (i = 0; i < line_count; i++)
  {
    if (line_kind[i] != LK_LABEL) continue;
    {
      char *name = line_text[i];
      int   f    = line_file_idx[i];
      int   d    = find_def(name);
      if (d < 0)
      {
        if (def_count >= MAX_DEFNAMES) { emsg("too many label defs"); return; }
        def_names[def_count] = name;
        def_files[def_count] = f;
        def_count++;
      }
      else if (def_files[d] != f)
      {
        if (is_global_name(name)) continue;
        if (is_conflict(name)) continue;
        if (conflict_count >= MAX_DEFNAMES) { emsg("too many conflicts"); return; }
        conflict_names[conflict_count++] = name;
      }
    }
  }
}

/* Rewrite a LK_INSTR line's text by replacing whole-word occurrences of every
 * conflicting label that this line's owning file defines (non-globally) with
 * `__<prefix>__<name>`. Also rewrite label refs in LK_DW_LABREF, LK_INT_LABREF,
 * LK_LABEL (definition itself), LK_ADDR2REG.
 *
 * We do this once per file (each file has a fixed set of conflicts to rewrite).
 */
static int file_owns_conflict(int file_idx, const char *name)
{
  int i;
  for (i = 0; i < line_count; i++)
  {
    if (line_kind[i] == LK_LABEL && line_file_idx[i] == file_idx)
    {
      if (strcmp(line_text[i], name) == 0) return 1;
    }
  }
  return 0;
}

static void pass_rewrite_conflicts(void)
{
  int ci;
  if (conflict_count == 0) return;

  for (ci = 0; ci < conflict_count; ci++)
  {
    char *name = conflict_names[ci];
    int   fi;
    /* For each file that owns this label (i.e. defines it), rewrite all
     * references in lines belonging to that file.
     * Note: a conflicting label may be defined in multiple files; each file
     * gets its own prefix.
     */
    for (fi = 0; fi < num_files; fi++)
    {
      int li;
      char tmp[MAX_LINE_LEN];
      if (!file_owns_conflict(fi, name)) continue;
      for (li = 0; li < line_count; li++)
      {
        if (line_file_idx[li] != fi) continue;
        switch (line_kind[li])
        {
          case LK_INSTR:
          case LK_LABEL:
            if (line_text[li] == NULL) break;
            if (rewrite_label_word(line_text[li], tmp, sizeof(tmp), name,
                                   file_prefix[fi]))
            {
              line_text[li] = pool_strdup(tmp);
            }
            break;
          case LK_DW_LABREF:
          case LK_INT_LABREF:
          case LK_ADDR2REG:
            if (line_label[li] && strcmp(line_label[li], name) == 0)
            {
              char nm[LABEL_NAME_LEN];
              snprintf(nm, sizeof(nm), "__%s__%s", file_prefix[fi], name);
              line_label[li] = pool_strdup(nm);
            }
            break;
          default:
            break;
        }
      }
    }
  }
}

/*===========================================================================*/
/*  Pass 3: prepend header (jump Main, nop, .dw 0)                           */
/*===========================================================================*/

static void prepend_header(void)
{
  /* We need to insert 3 lines at index 0. Easiest: shift existing lines right
   * by 3 and fill 0..2 with header records. This is O(N); N is small enough.
   */
  int i;
  if (line_count + 3 > MAX_LINES) { emsg("too many lines for header"); return; }
  for (i = line_count - 1; i >= 0; i--)
  {
    line_kind[i + 3]      = line_kind[i];
    line_text[i + 3]      = line_text[i];
    line_section[i + 3]   = line_section[i];
    line_file_idx[i + 3]  = line_file_idx[i];
    line_value[i + 3]     = line_value[i];
    line_label[i + 3]     = line_label[i];
    line_label_off[i + 3] = line_label_off[i];
    line_byte_off[i + 3]  = line_byte_off[i];
    line_byte_len[i + 3]  = line_byte_len[i];
    line_align[i + 3]     = line_align[i];
    line_dir_sec[i + 3]   = line_dir_sec[i];
    line_addr[i + 3]      = line_addr[i];
    line_flags[i + 3]     = line_flags[i];
  }
  line_count += 3;

  /* word 0: jump Main — kept absolute and relocated as RELOC_JUMP */
  line_kind[0]      = LK_INSTR;
  line_text[0]      = pool_strdup("jump Main");
  line_section[0]   = SEC_CODE;
  line_file_idx[0]  = 0;
  line_value[0]     = 0;
  line_label[0]     = NULL;
  line_label_off[0] = 0;
  line_byte_off[0]  = 0;
  line_byte_len[0]  = 0;
  line_align[0]     = 0;
  line_dir_sec[0]   = SEC_CODE;
  line_addr[0]      = 0;
  line_flags[0]     = FLAG_HEADER;

  /* word 1: nop (we always emit nop here for userBDOS — the BDOS loader does
   * not call user-program interrupt handlers; mirrors ASMPY independent=True)
   */
  line_kind[1]      = LK_INSTR;
  line_text[1]      = pool_strdup("nop");
  line_section[1]   = SEC_CODE;
  line_file_idx[1]  = 0;
  line_value[1]     = 0;
  line_label[1]     = NULL;
  line_label_off[1] = 0;
  line_byte_off[1]  = 0;
  line_byte_len[1]  = 0;
  line_align[1]     = 0;
  line_dir_sec[1]   = SEC_CODE;
  line_addr[1]      = 0;
  line_flags[1]     = 0;

  /* word 2: .dw 0 — patched to program_size after layout */
  line_kind[2]      = LK_DW_NUM;
  line_text[2]      = NULL;
  line_section[2]   = SEC_CODE;
  line_file_idx[2]  = 0;
  line_value[2]     = 0;
  line_label[2]     = NULL;
  line_label_off[2] = 0;
  line_byte_off[2]  = 0;
  line_byte_len[2]  = 0;
  line_align[2]     = 0;
  line_dir_sec[2]   = SEC_CODE;
  line_addr[2]      = 0;
  line_flags[2]     = 0;
}

/*===========================================================================*/
/*  Pass 4: expand pseudo-instructions (load32, addr2reg)                    */
/*===========================================================================*/

/* Expand a single addr2reg/load32 record into 1-2 records by inserting at the
 * given index. We do this by shifting the rest of the array; for clarity over
 * speed.
 */
static int insert_blank(int idx)
{
  int i;
  if (line_count >= MAX_LINES) { emsg("line array full"); return -1; }
  for (i = line_count - 1; i >= idx; i--)
  {
    line_kind[i + 1]      = line_kind[i];
    line_text[i + 1]      = line_text[i];
    line_section[i + 1]   = line_section[i];
    line_file_idx[i + 1]  = line_file_idx[i];
    line_value[i + 1]     = line_value[i];
    line_label[i + 1]     = line_label[i];
    line_label_off[i + 1] = line_label_off[i];
    line_byte_off[i + 1]  = line_byte_off[i];
    line_byte_len[i + 1]  = line_byte_len[i];
    line_align[i + 1]     = line_align[i];
    line_dir_sec[i + 1]   = line_dir_sec[i];
    line_addr[i + 1]      = line_addr[i];
    line_flags[i + 1]     = line_flags[i];
  }
  line_count++;
  /* Zero new slot */
  line_kind[idx]      = 0;
  line_text[idx]      = NULL;
  line_section[idx]   = SEC_CODE;
  line_file_idx[idx]  = 0;
  line_value[idx]     = 0;
  line_label[idx]     = NULL;
  line_label_off[idx] = 0;
  line_byte_off[idx]  = 0;
  line_byte_len[idx]  = 0;
  line_align[idx]     = 0;
  line_dir_sec[idx]   = SEC_CODE;
  line_addr[idx]      = 0;
  line_flags[idx]     = 0;
  return 0;
}

static void pass_expand_pseudo(void)
{
  int i;
  for (i = 0; i < line_count; i++)
  {
    if (line_kind[i] == LK_LOAD32)
    {
      unsigned int v = line_value[i];
      int reg = line_label_off[i];
      int high = (int)((v >> 16) & 0xFFFF);
      int low  = (int)(v & 0xFFFF);
      char buf[64];
      int sec = line_section[i];
      int fi  = line_file_idx[i];

      /* Replace this LK_LOAD32 with `load LOW reg` */
      line_kind[i] = LK_INSTR;
      snprintf(buf, sizeof(buf), "load %d r%d", low, reg);
      line_text[i] = pool_strdup(buf);

      if (high != 0)
      {
        if (insert_blank(i + 1) < 0) return;
        line_kind[i + 1]     = LK_INSTR;
        snprintf(buf, sizeof(buf), "loadhi %d r%d", high, reg);
        line_text[i + 1]     = pool_strdup(buf);
        line_section[i + 1]  = sec;
        line_file_idx[i + 1] = fi;
      }
    }
    else if (line_kind[i] == LK_ADDR2REG)
    {
      char *label_name = line_label[i];
      int   off  = line_label_off[i];
      int   reg  = (int)line_value[i];
      int   sec  = line_section[i];
      int   fi   = line_file_idx[i];

      line_kind[i]       = LK_LOAD_LABEL;
      line_label[i]      = label_name;
      line_label_off[i]  = off;
      line_value[i]      = (unsigned int)reg;
      line_text[i]       = NULL;
      line_flags[i]     |= FLAG_LOAD_PAIR;

      if (insert_blank(i + 1) < 0) return;
      line_kind[i + 1]      = LK_LOADHI_LABEL;
      line_label[i + 1]     = label_name;
      line_label_off[i + 1] = off;
      line_value[i + 1]     = (unsigned int)reg;
      line_section[i + 1]   = sec;
      line_file_idx[i + 1]  = fi;
      i++;  /* skip the loadhi */
    }
  }
}

/*===========================================================================*/
/*  Pass 5: pack ELF byte streams into .dw words                             */
/*  Consecutive LK_BYTES lines (same section) accumulate into one byte run;  */
/*  it is flushed as packed LK_DW_NUM lines on any boundary (label,          */
/*  directive, instruction, alignment, LK_INT_LABREF, LK_DW_*, etc.)         */
/*===========================================================================*/

/* Build a brand-new line array, walking the existing one. We use a two-list
 * approach: read from old, write to new. Since arrays are parallel, easier to
 * implement in place by collecting indices in an order array — but simplest:
 * allocate parallel "out" arrays, copy as we go, then swap pointers at the end.
 */
static int   *out_kind;
static char **out_text;
static int   *out_section;
static int   *out_file_idx;
static unsigned int *out_value;
static char **out_label;
static int   *out_label_off;
static int   *out_byte_off;
static int   *out_byte_len;
static int   *out_align;
static int   *out_dir_sec;
static int   *out_addr;
static int   *out_flags;
static int    out_count;

static void out_clone_from(int src)
{
  out_kind[out_count]      = line_kind[src];
  out_text[out_count]      = line_text[src];
  out_section[out_count]   = line_section[src];
  out_file_idx[out_count]  = line_file_idx[src];
  out_value[out_count]     = line_value[src];
  out_label[out_count]     = line_label[src];
  out_label_off[out_count] = line_label_off[src];
  out_byte_off[out_count]  = line_byte_off[src];
  out_byte_len[out_count]  = line_byte_len[src];
  out_align[out_count]     = line_align[src];
  out_dir_sec[out_count]   = line_dir_sec[src];
  out_addr[out_count]      = 0;
  out_flags[out_count]     = line_flags[src];
  out_count++;
}

static void out_emit_dw(unsigned int v, int section, int file_idx)
{
  out_kind[out_count]      = LK_DW_NUM;
  out_text[out_count]      = NULL;
  out_section[out_count]   = section;
  out_file_idx[out_count]  = file_idx;
  out_value[out_count]     = v;
  out_label[out_count]     = NULL;
  out_label_off[out_count] = 0;
  out_byte_off[out_count]  = 0;
  out_byte_len[out_count]  = 0;
  out_align[out_count]     = 0;
  out_dir_sec[out_count]   = section;
  out_addr[out_count]      = 0;
  out_flags[out_count]     = 0;
  out_count++;
}

/* Flush a pending byte buffer as packed words. The buffer is described by
 * a starting offset (within byte_pool) and a length; we copy in order.
 * Note: bytes from possibly multiple LK_BYTES lines are NOT contiguous in
 * byte_pool, so we accumulate into a temporary.
 */
static unsigned char pack_tmp[BYTE_POOL_BYTES];
static int           pack_len;
static int           pack_section;
static int           pack_file_idx;

static void pack_flush(void)
{
  int i;
  if (pack_len == 0) return;
  while (pack_len % 4 != 0) pack_tmp[pack_len++] = 0;
  for (i = 0; i < pack_len; i += 4)
  {
    unsigned int w = (unsigned int)pack_tmp[i]
                   | ((unsigned int)pack_tmp[i + 1] << 8)
                   | ((unsigned int)pack_tmp[i + 2] << 16)
                   | ((unsigned int)pack_tmp[i + 3] << 24);
    out_emit_dw(w, pack_section, pack_file_idx);
  }
  pack_len = 0;
}

static void pack_append(int byte_off, int byte_len, int section, int file_idx)
{
  int k;
  if (pack_len == 0)
  {
    pack_section = section;
    pack_file_idx = file_idx;
  }
  for (k = 0; k < byte_len; k++)
  {
    if (pack_len >= (int)sizeof(pack_tmp)) { emsg("pack buffer overflow"); return; }
    pack_tmp[pack_len++] = byte_pool[byte_off + k];
  }
}

static int alloc_out_arrays(void)
{
  out_kind      = (int *)IO_HEAP_ALLOC(sizeof(int) * MAX_LINES);
  out_text      = (char **)IO_HEAP_ALLOC(sizeof(char *) * MAX_LINES);
  out_section   = (int *)IO_HEAP_ALLOC(sizeof(int) * MAX_LINES);
  out_file_idx  = (int *)IO_HEAP_ALLOC(sizeof(int) * MAX_LINES);
  out_value     = (unsigned int *)IO_HEAP_ALLOC(sizeof(unsigned int) * MAX_LINES);
  out_label     = (char **)IO_HEAP_ALLOC(sizeof(char *) * MAX_LINES);
  out_label_off = (int *)IO_HEAP_ALLOC(sizeof(int) * MAX_LINES);
  out_byte_off  = (int *)IO_HEAP_ALLOC(sizeof(int) * MAX_LINES);
  out_byte_len  = (int *)IO_HEAP_ALLOC(sizeof(int) * MAX_LINES);
  out_align     = (int *)IO_HEAP_ALLOC(sizeof(int) * MAX_LINES);
  out_dir_sec   = (int *)IO_HEAP_ALLOC(sizeof(int) * MAX_LINES);
  out_addr      = (int *)IO_HEAP_ALLOC(sizeof(int) * MAX_LINES);
  out_flags     = (int *)IO_HEAP_ALLOC(sizeof(int) * MAX_LINES);

  if (!out_kind || !out_text || !out_section || !out_file_idx ||
      !out_value || !out_label || !out_label_off || !out_byte_off ||
      !out_byte_len || !out_align || !out_dir_sec || !out_addr || !out_flags)
  {
    emsg("OOM allocating output line arrays");
    return -1;
  }
  return 0;
}

static void swap_out_to_in(void)
{
  int *t_int;
  char **t_pp;
  unsigned int *t_uint;

  t_int = line_kind;      line_kind = out_kind;           out_kind = t_int;
  t_pp  = line_text;      line_text = out_text;           out_text = t_pp;
  t_int = line_section;   line_section = out_section;     out_section = t_int;
  t_int = line_file_idx;  line_file_idx = out_file_idx;   out_file_idx = t_int;
  t_uint = line_value;    line_value = out_value;         out_value = t_uint;
  t_pp  = line_label;     line_label = out_label;         out_label = t_pp;
  t_int = line_label_off; line_label_off = out_label_off; out_label_off = t_int;
  t_int = line_byte_off;  line_byte_off = out_byte_off;   out_byte_off = t_int;
  t_int = line_byte_len;  line_byte_len = out_byte_len;   out_byte_len = t_int;
  t_int = line_align;     line_align = out_align;         out_align = t_int;
  t_int = line_dir_sec;   line_dir_sec = out_dir_sec;     out_dir_sec = t_int;
  t_int = line_addr;      line_addr = out_addr;           out_addr = t_int;
  t_int = line_flags;     line_flags = out_flags;         out_flags = t_int;
  line_count = out_count;
}

static void pass_pack_elf(void)
{
  int i;
  out_count = 0;
  pack_len = 0;
  for (i = 0; i < line_count; i++)
  {
    if (line_kind[i] == LK_BYTES)
    {
      pack_append(line_byte_off[i], line_byte_len[i],
                  line_section[i], line_file_idx[i]);
    }
    else
    {
      pack_flush();
      out_clone_from(i);
    }
  }
  pack_flush();
  swap_out_to_in();
}

/*===========================================================================*/
/*  Pass 6: drop globals and directives, sort by section (stable),           */
/*          handle .balign by inserting padding NOPs / zero words.           */
/*===========================================================================*/

static void pass_remove_meta_and_sort(void)
{
  int sec;
  int i;
  out_count = 0;

  /* For each section in order, copy lines with that section.
   * Skip LK_GLOBAL and LK_DIR entirely. Keep LK_BALIGN for now (handled later).
   *
   * Stable per-section ordering preserves input order within each section,
   * which is what ASMPY does (Python's sorted() is stable).
   */
  for (sec = 0; sec < NUM_SECTIONS; sec++)
  {
    for (i = 0; i < line_count; i++)
    {
      if (line_section[i] != sec) continue;
      if (line_kind[i] == LK_GLOBAL) continue;
      if (line_kind[i] == LK_DIR)    continue;
      out_clone_from(i);
    }
  }
  swap_out_to_in();
}

/* Pass 7: handle .balign.
 *
 * Each line is one word (4 bytes). We track the running byte address. When we
 * encounter a LK_BALIGN, we need to advance to a multiple of `n`. Since each
 * "real" line emits 4 bytes, alignment to 1, 2, or 4 is automatic. Higher
 * alignments would require padding by emitting extra words. We assert n <= 4
 * for now (QBE only emits .balign 1 and .balign 4).
 *
 * For convenience we just drop LK_BALIGN lines after verification.
 */
static void pass_handle_balign(void)
{
  int i;
  out_count = 0;
  for (i = 0; i < line_count; i++)
  {
    if (line_kind[i] == LK_BALIGN)
    {
      int n = line_align[i];
      if (n > 4)
      {
        emsg(".balign > 4 not supported by asm-link");
        return;
      }
      continue;
    }
    out_clone_from(i);
  }
  swap_out_to_in();
}

/*===========================================================================*/
/*  Pass 8: extract LK_LABEL records into the label table, removing them.    */
/*===========================================================================*/

static void pass_extract_labels(void)
{
  int i;
  int byte_addr = 0;
  int idx;
  out_count = 0;

  /* First pass: walk lines and assign byte addresses (each non-label line = 4
   * bytes); when a LK_LABEL appears, record (name -> byte_addr).
   */
  for (i = 0; i < line_count; i++)
  {
    if (line_kind[i] == LK_LABEL)
    {
      idx = add_label(line_text[i], line_file_idx[i]);
      if (idx < 0) return;
      label_addr[idx] = byte_addr;
      if (is_global_name(line_text[i])) label_global[idx] = 1;
      continue;  /* drop label line */
    }
    out_clone_from(i);
    line_addr[i] = byte_addr;     /* not used after swap, but harmless */
    out_addr[out_count - 1] = byte_addr;
    byte_addr += 4;
  }
  swap_out_to_in();
}

/*===========================================================================*/
/*  Pass 9: encode all instructions and data words to binary, tracking       */
/*          relocation entries.                                              */
/*===========================================================================*/

static int find_label_or_err(const char *name)
{
  int idx = find_label(name);
  if (idx < 0) emsg2("undefined symbol: ", name);
  return idx;
}

/* Add a relocation entry for byte_offset in the output, of given type. */
static void add_reloc(int byte_offset, int type)
{
  if (reloc_count >= MAX_RELOCS) { emsg("too many relocations"); return; }
  reloc_entries[reloc_count++] =
    ((unsigned int)byte_offset << 8) | ((unsigned int)type & 0xFFu);
}

/* Encode load (or loadhi) of label-low/label-high. Called for both
 * LK_LOAD_LABEL and LK_LOADHI_LABEL.
 */
static unsigned int encode_load_label(int li, int high_half)
{
  int reg = (int)line_value[li];
  int lab_idx = find_label_or_err(line_label[li]);
  int target;
  unsigned int hi;
  unsigned int op;
  if (lab_idx < 0) return 0;
  target = label_addr[lab_idx] + line_label_off[li];
  if (high_half) hi = (unsigned int)((target >> 16) & 0xFFFF);
  else           hi = (unsigned int)(target & 0xFFFF);
  op = high_half ? ARITH_LOADHI : ARITH_LOAD;
  return (OP_ARITHC << 28) | (op << 24) | (hi << 8) |
         ((unsigned int)(reg & 0xF) << 4) | (unsigned int)(reg & 0xF);
}

/* Encode an LK_INSTR line. Some instructions reference labels:
 *   - jump LABEL (header): keep absolute, reloc as RELOC_JUMP
 *   - jump LABEL (non-header): convert to jumpo with relative offset
 *   - branch LABEL: relative offset
 *   - load/loadhi LABEL: pair (we handle this via LK_LOAD_LABEL after expansion)
 */
static unsigned int encode_instr(int li, int byte_addr)
{
  char tmp[MAX_LINE_LEN];
  char *toks[MAX_TOKENS];
  int  nt;
  const char *m;
  int reg_a, reg_b, reg_d;
  int val;
  int arith;

  /* copy to mutable buffer */
  {
    int k = 0;
    while (line_text[li][k] && k < (int)sizeof(tmp) - 1) { tmp[k] = line_text[li][k]; k++; }
    tmp[k] = 0;
  }
  nt = tokenize(tmp, toks, MAX_TOKENS);
  if (nt == 0) { emsg("empty instruction"); return 0; }
  m = toks[0];

  /* ---- Control ---- */
  if (strcmp(m, "halt") == 0) return 0xFFFFFFFFu;
  if (strcmp(m, "nop") == 0)  return 0u;
  if (strcmp(m, "ccache") == 0) return (OP_CCACHE << 28);
  if (strcmp(m, "reti") == 0)   return (OP_RETI << 28);
  if (strcmp(m, "savpc") == 0)
  {
    if (nt != 2) { emsg("savpc needs 1 arg"); return 0; }
    reg_d = parse_register(toks[1]);
    if (reg_d < 0) { emsg("savpc bad reg"); return 0; }
    return (OP_SAVPC << 28) | (unsigned)(reg_d & 0xF);
  }
  if (strcmp(m, "readintid") == 0)
  {
    if (nt != 2) { emsg("readintid needs 1 arg"); return 0; }
    reg_d = parse_register(toks[1]);
    if (reg_d < 0) { emsg("readintid bad reg"); return 0; }
    return (OP_INTID << 28) | (unsigned)(reg_d & 0xF);
  }

  /* ---- Memory: word ---- */
  if (strcmp(m, "read") == 0 || strcmp(m, "readb") == 0 ||
      strcmp(m, "readbu") == 0 || strcmp(m, "readh") == 0 ||
      strcmp(m, "readhu") == 0)
  {
    unsigned int subop;
    if (nt != 4) { emsg("read* needs 3 args"); return 0; }
    if (!parse_number(toks[1], &val)) { emsg("read* bad imm"); return 0; }
    reg_a = parse_register(toks[2]);
    reg_d = parse_register(toks[3]);
    if (reg_a < 0 || reg_d < 0) { emsg("read* bad reg"); return 0; }
    if (m[4] == 0)         subop = MEM_WORD;
    else if (m[4] == 'b' && m[5] == 0)  subop = MEM_BYTE;
    else if (m[4] == 'b' && m[5] == 'u') subop = MEM_BYTE_U;
    else if (m[4] == 'h' && m[5] == 0)  subop = MEM_HALF;
    else if (m[4] == 'h' && m[5] == 'u') subop = MEM_HALF_U;
    else { emsg("bad read variant"); return 0; }
    return (OP_READ << 28) | (((unsigned)val & 0xFFFFu) << 12) |
           (((unsigned)reg_a & 0xFu) << 8) | (subop << 4) |
           ((unsigned)reg_d & 0xFu);
  }
  if (strcmp(m, "write") == 0 || strcmp(m, "writeb") == 0 ||
      strcmp(m, "writeh") == 0)
  {
    unsigned int subop;
    if (nt != 4) { emsg("write* needs 3 args"); return 0; }
    if (!parse_number(toks[1], &val)) { emsg("write* bad imm"); return 0; }
    reg_a = parse_register(toks[2]);
    reg_b = parse_register(toks[3]);
    if (reg_a < 0 || reg_b < 0) { emsg("write* bad reg"); return 0; }
    if (m[5] == 0)              subop = MEM_WORD;
    else if (m[5] == 'b')       subop = MEM_BYTE;
    else if (m[5] == 'h')       subop = MEM_HALF;
    else { emsg("bad write variant"); return 0; }
    return (OP_WRITE << 28) | (((unsigned)val & 0xFFFFu) << 12) |
           (((unsigned)reg_a & 0xFu) << 8) | (((unsigned)reg_b & 0xFu) << 4) |
           subop;
  }
  if (strcmp(m, "push") == 0)
  {
    if (nt != 2) { emsg("push needs 1 arg"); return 0; }
    reg_b = parse_register(toks[1]);
    if (reg_b < 0) { emsg("push bad reg"); return 0; }
    return (OP_PUSH << 28) | (((unsigned)reg_b & 0xFu) << 4);
  }
  if (strcmp(m, "pop") == 0)
  {
    if (nt != 2) { emsg("pop needs 1 arg"); return 0; }
    reg_d = parse_register(toks[1]);
    if (reg_d < 0) { emsg("pop bad reg"); return 0; }
    return (OP_POP << 28) | ((unsigned)reg_d & 0xFu);
  }

  /* ---- Jump ---- */
  if (strcmp(m, "jump") == 0)
  {
    int target;
    int is_header = (line_flags[li] & FLAG_HEADER) != 0;
    if (nt != 2) { emsg("jump needs 1 arg"); return 0; }
    if (toks[1][0] >= '0' && toks[1][0] <= '9')
    {
      /* numeric absolute */
      if (!parse_number(toks[1], &val)) { emsg("jump bad imm"); return 0; }
      return (OP_JUMP << 28) | (((unsigned)val & 0x7FFFFFFu) << 1);
    }
    /* label */
    {
      int lab_idx = find_label_or_err(toks[1]);
      if (lab_idx < 0) return 0;
      target = label_addr[lab_idx];
    }
    if (is_header)
    {
      /* Keep absolute, mark relocatable */
      add_reloc(byte_addr, RELOC_JUMP);
      return (OP_JUMP << 28) | (((unsigned)target & 0x7FFFFFFu) << 1);
    }
    else
    {
      int rel = target - byte_addr;
      return (OP_JUMP << 28) | (((unsigned)rel & 0x7FFFFFFu) << 1) | 1u;
    }
  }
  if (strcmp(m, "jumpo") == 0)
  {
    if (nt != 2) { emsg("jumpo needs 1 arg"); return 0; }
    if (!parse_number(toks[1], &val)) { emsg("jumpo bad imm"); return 0; }
    return (OP_JUMP << 28) | (((unsigned)val & 0x7FFFFFFu) << 1) | 1u;
  }
  if (strcmp(m, "jumpr") == 0)
  {
    if (nt != 3) { emsg("jumpr needs 2 args"); return 0; }
    if (!parse_number(toks[1], &val)) { emsg("jumpr bad imm"); return 0; }
    reg_b = parse_register(toks[2]);
    if (reg_b < 0) { emsg("jumpr bad reg"); return 0; }
    return (OP_JUMPR << 28) | (((unsigned)val & 0xFFFFu) << 12) |
           (((unsigned)reg_b & 0xFu) << 4);
  }
  if (strcmp(m, "jumpro") == 0)
  {
    if (nt != 3) { emsg("jumpro needs 2 args"); return 0; }
    if (!parse_number(toks[1], &val)) { emsg("jumpro bad imm"); return 0; }
    reg_b = parse_register(toks[2]);
    if (reg_b < 0) { emsg("jumpro bad reg"); return 0; }
    return (OP_JUMPR << 28) | (((unsigned)val & 0xFFFFu) << 12) |
           (((unsigned)reg_b & 0xFu) << 4) | 1u;
  }

  /* ---- Branches ---- */
  {
    unsigned br_op = 0xFFu;
    int signed_flag = 0;
    if (strcmp(m, "beq")  == 0) { br_op = BR_BEQ; }
    else if (strcmp(m, "bne")  == 0) { br_op = BR_BNE; }
    else if (strcmp(m, "bgt")  == 0) { br_op = BR_BGT; }
    else if (strcmp(m, "bge")  == 0) { br_op = BR_BGE; }
    else if (strcmp(m, "blt")  == 0) { br_op = BR_BLT; }
    else if (strcmp(m, "ble")  == 0) { br_op = BR_BLE; }
    else if (strcmp(m, "bgts") == 0) { br_op = BR_BGT; signed_flag = 1; }
    else if (strcmp(m, "bges") == 0) { br_op = BR_BGE; signed_flag = 1; }
    else if (strcmp(m, "blts") == 0) { br_op = BR_BLT; signed_flag = 1; }
    else if (strcmp(m, "bles") == 0) { br_op = BR_BLE; signed_flag = 1; }
    if (br_op != 0xFFu)
    {
      int rel;
      if (nt != 4) { emsg("branch needs 3 args"); return 0; }
      reg_a = parse_register(toks[1]);
      reg_b = parse_register(toks[2]);
      if (reg_a < 0 || reg_b < 0) { emsg("branch bad reg"); return 0; }
      if (toks[3][0] >= '0' && toks[3][0] <= '9' ||
          toks[3][0] == '-' || toks[3][0] == '+')
      {
        if (!parse_number(toks[3], &val)) { emsg("branch bad imm"); return 0; }
        rel = val;
      }
      else
      {
        int lab_idx = find_label_or_err(toks[3]);
        if (lab_idx < 0) return 0;
        rel = label_addr[lab_idx] - byte_addr;
      }
      return (OP_BRANCH << 28) | (((unsigned)rel & 0xFFFFu) << 12) |
             (((unsigned)reg_a & 0xFu) << 8) | (((unsigned)reg_b & 0xFu) << 4) |
             ((br_op & 0x7u) << 1) | (unsigned)(signed_flag & 1);
    }
  }

  /* ---- Single-cycle arithmetic ---- */
  arith = -1;
  if      (strcmp(m, "or") == 0)      arith = ARITH_OR;
  else if (strcmp(m, "and") == 0)     arith = ARITH_AND;
  else if (strcmp(m, "xor") == 0)     arith = ARITH_XOR;
  else if (strcmp(m, "add") == 0)     arith = ARITH_ADD;
  else if (strcmp(m, "sub") == 0)     arith = ARITH_SUB;
  else if (strcmp(m, "shiftl") == 0)  arith = ARITH_SHIFTL;
  else if (strcmp(m, "shiftr") == 0)  arith = ARITH_SHIFTR;
  else if (strcmp(m, "shiftrs") == 0) arith = ARITH_SHIFTRS;
  else if (strcmp(m, "slt") == 0)     arith = ARITH_SLT;
  else if (strcmp(m, "sltu") == 0)    arith = ARITH_SLTU;

  if (arith >= 0)
  {
    if (nt != 4) { emsg2("arith needs 3 args: ", line_text[li]); return 0; }
    reg_a = parse_register(toks[1]);
    reg_d = parse_register(toks[3]);
    if (reg_a < 0 || reg_d < 0) { emsg("arith bad reg"); return 0; }
    reg_b = parse_register(toks[2]);
    if (reg_b >= 0)
    {
      return (OP_ARITH << 28) | ((unsigned)(arith & 0xF) << 24) |
             (((unsigned)reg_a & 0xFu) << 8) | (((unsigned)reg_b & 0xFu) << 4) |
             ((unsigned)reg_d & 0xFu);
    }
    if (!parse_number(toks[2], &val)) { emsg2("arith bad imm: ", line_text[li]); return 0; }
    return (OP_ARITHC << 28) | ((unsigned)(arith & 0xF) << 24) |
           (((unsigned)val & 0xFFFFu) << 8) | (((unsigned)reg_a & 0xFu) << 4) |
           ((unsigned)reg_d & 0xFu);
  }

  if (strcmp(m, "not") == 0)
  {
    if (nt != 3) { emsg("not needs 2 args"); return 0; }
    reg_a = parse_register(toks[1]);
    reg_d = parse_register(toks[2]);
    if (reg_a < 0 || reg_d < 0) { emsg("not bad reg"); return 0; }
    return (OP_ARITH << 28) | (ARITH_NOT << 24) |
           (((unsigned)reg_a & 0xFu) << 8) | ((unsigned)reg_d & 0xFu);
  }

  if (strcmp(m, "load") == 0)
  {
    if (nt != 3) { emsg("load needs 2 args"); return 0; }
    reg_d = parse_register(toks[2]);
    if (reg_d < 0) { emsg("load bad reg"); return 0; }
    if (toks[1][0] >= '0' && toks[1][0] <= '9' ||
        toks[1][0] == '-' || toks[1][0] == '+')
    {
      if (!parse_number(toks[1], &val)) { emsg("load bad imm"); return 0; }
    }
    else
    {
      /* label argument — record reloc as load+loadhi pair */
      int lab_idx = find_label_or_err(toks[1]);
      if (lab_idx < 0) return 0;
      val = label_addr[lab_idx] & 0xFFFF;
      add_reloc(byte_addr, RELOC_LOAD_PAIR);
    }
    return (OP_ARITHC << 28) | (ARITH_LOAD << 24) |
           (((unsigned)val & 0xFFFFu) << 8) | (((unsigned)reg_d & 0xFu) << 4) |
           ((unsigned)reg_d & 0xFu);
  }

  if (strcmp(m, "loadhi") == 0)
  {
    if (nt != 3) { emsg("loadhi needs 2 args"); return 0; }
    reg_d = parse_register(toks[2]);
    if (reg_d < 0) { emsg("loadhi bad reg"); return 0; }
    if (toks[1][0] >= '0' && toks[1][0] <= '9' ||
        toks[1][0] == '-' || toks[1][0] == '+')
    {
      if (!parse_number(toks[1], &val)) { emsg("loadhi bad imm"); return 0; }
    }
    else
    {
      int lab_idx = find_label_or_err(toks[1]);
      if (lab_idx < 0) return 0;
      val = (label_addr[lab_idx] >> 16) & 0xFFFF;
      /* No reloc here; the paired load already recorded RELOC_LOAD_PAIR. */
    }
    return (OP_ARITHC << 28) | (ARITH_LOADHI << 24) |
           (((unsigned)val & 0xFFFFu) << 8) | (((unsigned)reg_d & 0xFu) << 4) |
           ((unsigned)reg_d & 0xFu);
  }

  /* ---- Multi-cycle arithmetic + FP64 ---- */
  arith = -1;
  if      (strcmp(m, "mults")   == 0) arith = ARITHM_MULTS;
  else if (strcmp(m, "multu")   == 0) arith = ARITHM_MULTU;
  else if (strcmp(m, "multfp")  == 0) arith = ARITHM_MULTFP;
  else if (strcmp(m, "divs")    == 0) arith = ARITHM_DIVS;
  else if (strcmp(m, "divu")    == 0) arith = ARITHM_DIVU;
  else if (strcmp(m, "divfp")   == 0) arith = ARITHM_DIVFP;
  else if (strcmp(m, "mods")    == 0) arith = ARITHM_MODS;
  else if (strcmp(m, "modu")    == 0) arith = ARITHM_MODU;
  else if (strcmp(m, "fmul")    == 0) arith = ARITHM_FMUL;
  else if (strcmp(m, "fadd")    == 0) arith = ARITHM_FADD;
  else if (strcmp(m, "fsub")    == 0) arith = ARITHM_FSUB;
  else if (strcmp(m, "fld")     == 0) arith = ARITHM_FLD;
  else if (strcmp(m, "fsthi")   == 0) arith = ARITHM_FSTHI;
  else if (strcmp(m, "fstlo")   == 0) arith = ARITHM_FSTLO;
  else if (strcmp(m, "mulshi")  == 0) arith = ARITHM_MULSHI;
  else if (strcmp(m, "multuhi") == 0) arith = ARITHM_MULTUHI;

  if (arith >= 0)
  {
    if (nt != 4) { emsg2("multi-cycle arith needs 3 args: ", line_text[li]); return 0; }
    reg_a = parse_register(toks[1]);
    reg_d = parse_register(toks[3]);
    if (reg_a < 0 || reg_d < 0) { emsg("mc-arith bad reg"); return 0; }
    reg_b = parse_register(toks[2]);
    if (reg_b >= 0)
    {
      return (OP_ARITHMC << 28) | ((unsigned)(arith & 0xF) << 24) |
             (((unsigned)reg_a & 0xFu) << 8) | (((unsigned)reg_b & 0xFu) << 4) |
             ((unsigned)reg_d & 0xFu);
    }
    if (!parse_number(toks[2], &val)) { emsg2("mc-arith bad imm: ", line_text[li]); return 0; }
    return (OP_ARITHM << 28) | ((unsigned)(arith & 0xF) << 24) |
           (((unsigned)val & 0xFFFFu) << 8) | (((unsigned)reg_a & 0xFu) << 4) |
           ((unsigned)reg_d & 0xFu);
  }

  emsg2("unknown instruction: ", line_text[li]);
  return 0;
}

/*===========================================================================*/
/*  Pass 9: encode all lines                                                 */
/*===========================================================================*/

static void pass_encode(void)
{
  int i;
  int byte_addr;

  output_count = 0;
  reloc_count = 0;
  byte_addr = 0;

  for (i = 0; i < line_count; i++)
  {
    unsigned int w = 0;
    switch (line_kind[i])
    {
      case LK_INSTR:
        w = encode_instr(i, byte_addr);
        break;

      case LK_DW_NUM:
        w = line_value[i];
        break;

      case LK_DW_LABREF:
      case LK_INT_LABREF:
      {
        int lab_idx = find_label_or_err(line_label[i]);
        if (lab_idx < 0) { has_error = 1; w = 0; break; }
        w = (unsigned int)(label_addr[lab_idx] + line_label_off[i]);
        add_reloc(byte_addr, RELOC_DATA_WORD);
        break;
      }

      case LK_LOAD_LABEL:
        w = encode_load_label(i, 0);
        if (line_flags[i] & FLAG_LOAD_PAIR)
          add_reloc(byte_addr, RELOC_LOAD_PAIR);
        break;

      case LK_LOADHI_LABEL:
        w = encode_load_label(i, 1);
        break;

      default:
        emsg("internal: unhandled line kind in encode pass");
        return;
    }
    if (output_count >= OUTPUT_WORDS) { emsg("output too large"); return; }
    output_words[output_count++] = w;
    byte_addr += 4;
    if (has_error) return;
  }

  /* Patch header word 2 with program size in words. */
  if (output_count >= 3)
  {
    output_words[2] = (unsigned int)output_count;
  }
}

/*===========================================================================*/
/*  Pass 10: append relocation table                                         */
/*===========================================================================*/

/* Sort reloc entries by byte_offset (ascending). Used to make output
 * deterministic. Selection sort is fine for the typical sizes (<= 16K).
 */
static void sort_relocs(void)
{
  int i, j, min_idx;
  unsigned int tmp;
  for (i = 0; i < reloc_count - 1; i++)
  {
    min_idx = i;
    for (j = i + 1; j < reloc_count; j++)
    {
      if (reloc_entries[j] < reloc_entries[min_idx]) min_idx = j;
    }
    if (min_idx != i)
    {
      tmp = reloc_entries[i];
      reloc_entries[i] = reloc_entries[min_idx];
      reloc_entries[min_idx] = tmp;
    }
  }
}

static void pass_append_reloc(void)
{
  int i;
  if (reloc_count == 0) return;
  sort_relocs();
  if (output_count + 1 + reloc_count > OUTPUT_WORDS)
  {
    emsg("output too large for reloc table");
    return;
  }
  output_words[output_count++] = (unsigned int)reloc_count;
  for (i = 0; i < reloc_count; i++)
  {
    output_words[output_count++] = reloc_entries[i];
  }
}

/*===========================================================================*/
/*  Output                                                                   */
/*===========================================================================*/

static int write_output(const char *path)
{
  int fd;
  int total = 0;
  int rem = 0;
  int chunk = 0;
  int wrote = 0;
  int i;

  IO_DELETE((char *)path);
  IO_CREATE((char *)path);
  fd = IO_OPEN((char *)path);
  if (fd < 0) { emsg2("cannot open output: ", path); return -1; }

#ifdef ASMLINK_HOST
  /* On host: emit big-endian byte stream so the result matches
   * `perl -ne 'print pack("B32", $_)'` from compile_modern_c.sh.
   * That gives the canonical .bin layout. We write 4 bytes per word, BE.
   */
  {
    unsigned char *buf = (unsigned char *)malloc((size_t)output_count * 4);
    for (i = 0; i < output_count; i++)
    {
      unsigned int v = output_words[i];
      buf[i * 4 + 0] = (unsigned char)((v >> 24) & 0xFF);
      buf[i * 4 + 1] = (unsigned char)((v >> 16) & 0xFF);
      buf[i * 4 + 2] = (unsigned char)((v >> 8) & 0xFF);
      buf[i * 4 + 3] = (unsigned char)(v & 0xFF);
    }
    fwrite(buf, 1, (size_t)output_count * 4, host_files[fd]);
    free(buf);
    IO_CLOSE(fd);
    (void)total; (void)rem; (void)chunk; (void)wrote;
    return output_count;
  }
#else
  /* On BDOS: write words directly. BRFS stores them as 32-bit words. */
  total = 0;
  rem = output_count;
  while (rem > 0)
  {
    chunk = rem;
    if (chunk > 256) chunk = 256;
    wrote = IO_WRITE_WORDS(fd, &output_words[total], chunk);
    if (wrote <= 0) { emsg("write failed"); IO_CLOSE(fd); return -1; }
    total += wrote;
    rem -= wrote;
  }
  IO_CLOSE(fd);
  return total;
#endif
}

/*===========================================================================*/
/*  Memory allocation for global tables                                      */
/*===========================================================================*/

static int allocate_buffers(void)
{
  str_pool_size = STR_POOL_BYTES;
  str_pool      = (char *)IO_HEAP_ALLOC(str_pool_size);
  str_pool_pos  = 0;

  byte_pool_size = BYTE_POOL_BYTES;
  byte_pool      = (unsigned char *)IO_HEAP_ALLOC(byte_pool_size);
  byte_pool_pos  = 0;

  line_kind      = (int *)IO_HEAP_ALLOC(sizeof(int) * MAX_LINES);
  line_text      = (char **)IO_HEAP_ALLOC(sizeof(char *) * MAX_LINES);
  line_section   = (int *)IO_HEAP_ALLOC(sizeof(int) * MAX_LINES);
  line_file_idx  = (int *)IO_HEAP_ALLOC(sizeof(int) * MAX_LINES);
  line_value     = (unsigned int *)IO_HEAP_ALLOC(sizeof(unsigned int) * MAX_LINES);
  line_label     = (char **)IO_HEAP_ALLOC(sizeof(char *) * MAX_LINES);
  line_label_off = (int *)IO_HEAP_ALLOC(sizeof(int) * MAX_LINES);
  line_byte_off  = (int *)IO_HEAP_ALLOC(sizeof(int) * MAX_LINES);
  line_byte_len  = (int *)IO_HEAP_ALLOC(sizeof(int) * MAX_LINES);
  line_align     = (int *)IO_HEAP_ALLOC(sizeof(int) * MAX_LINES);
  line_dir_sec   = (int *)IO_HEAP_ALLOC(sizeof(int) * MAX_LINES);
  line_addr      = (int *)IO_HEAP_ALLOC(sizeof(int) * MAX_LINES);
  line_flags     = (int *)IO_HEAP_ALLOC(sizeof(int) * MAX_LINES);

  label_names    = (char (*)[LABEL_NAME_LEN])IO_HEAP_ALLOC(MAX_LABELS * LABEL_NAME_LEN);
  label_addr     = (int *)IO_HEAP_ALLOC(sizeof(int) * MAX_LABELS);
  label_file     = (int *)IO_HEAP_ALLOC(sizeof(int) * MAX_LABELS);
  label_global   = (int *)IO_HEAP_ALLOC(sizeof(int) * MAX_LABELS);

  output_words   = (unsigned int *)IO_HEAP_ALLOC(sizeof(unsigned int) * OUTPUT_WORDS);
  reloc_entries  = (unsigned int *)IO_HEAP_ALLOC(sizeof(unsigned int) * MAX_RELOCS);

  if (!str_pool || !byte_pool || !line_kind || !line_text ||
      !label_names || !label_addr || !output_words || !reloc_entries)
  {
    emsg("OOM allocating buffers");
    return -1;
  }

  if (alloc_out_arrays() < 0) return -1;

  return 0;
}

/*===========================================================================*/
/*  Main                                                                     */
/*===========================================================================*/

static void usage(void)
{
  IO_PRINT("Usage: asm-link [-v] [-o output.bin] input1.asm [input2.asm ...]\n");
}

#ifdef ASMLINK_HOST
int main(int argc, char **argv)
{
  host_argc = argc;
  host_argv = argv;
#else
int main(void)
{
  int   argc = IO_ARGC();
  char **argv = IO_ARGV();
#endif
  int i;
  int rc;

  num_files = 0;
  output_path = NULL;
  verbose = 0;
  has_error = 0;

  if (argc < 2) { usage(); return 1; }

  /* Allocate buffers FIRST so pool_strdup works in arg parsing. */
  if (allocate_buffers() < 0) return 1;

  /* Parse args */
  for (i = 1; i < argc; i++)
  {
    if (strcmp(argv[i], "-o") == 0)
    {
      if (i + 1 >= argc) { usage(); return 1; }
      output_path = argv[++i];
    }
    else if (strcmp(argv[i], "-v") == 0)
    {
      verbose = 1;
    }
    else if (strcmp(argv[i], "-d") == 0)
    {
      dump_labels = 1;
    }
    else if (strcmp(argv[i], "-h") == 0 ||
             strcmp(argv[i], "--help") == 0)
    {
      usage();
      return 0;
    }
    else
    {
      if (num_files >= MAX_FILES) { emsg("too many input files"); return 1; }
      file_paths[num_files] = argv[i];
      file_prefix[num_files] = make_prefix(argv[i]);
      num_files++;
    }
  }

  if (num_files == 0) { usage(); return 1; }
  if (!output_path)   { usage(); return 1; }

  /* Pipeline */
  vmsg("Reading "); vmsg_int(num_files); vmsg(" input files\n");
  for (i = 0; i < num_files; i++)
  {
    if (parse_file(i) < 0 || has_error) goto done;
  }
  vmsg("Parsed "); vmsg_int(line_count); vmsg(" lines\n");

  pass_collect_globals();
  if (has_error) goto done;

  pass_find_conflicts();
  if (has_error) goto done;
  vmsg("Found "); vmsg_int(conflict_count); vmsg(" cross-file label conflicts\n");

  pass_rewrite_conflicts();
  if (has_error) goto done;

  prepend_header();
  if (has_error) goto done;

  pass_expand_pseudo();
  if (has_error) goto done;

  pass_pack_elf();
  if (has_error) goto done;
  vmsg("After ELF pack: "); vmsg_int(line_count); vmsg(" lines\n");

  pass_remove_meta_and_sort();
  if (has_error) goto done;

  pass_handle_balign();
  if (has_error) goto done;

  pass_extract_labels();
  if (has_error) goto done;
  vmsg("Labels: "); vmsg_int(label_count); vmsg("\n");
  vmsg("Program lines: "); vmsg_int(line_count); vmsg("\n");

  if (dump_labels)
  {
    int li;
    char buf[128];
    for (li = 0; li < label_count; li++)
    {
      snprintf(buf, sizeof(buf), "LBL %08x %s\n",
               (unsigned)label_addr[li], label_names[li]);
      IO_PRINT(buf);
    }
  }

  pass_encode();
  if (has_error) goto done;
  vmsg("Encoded "); vmsg_int(output_count); vmsg(" words, ");
  vmsg_int(reloc_count); vmsg(" relocations\n");

  pass_append_reloc();
  if (has_error) goto done;

  rc = write_output(output_path);
  if (rc < 0) goto done;
  vmsg("Wrote "); vmsg_int(output_count); vmsg(" words to "); vmsg(output_path); vmsg("\n");

done:
  if (has_error)
  {
    IO_PRINT("asm-link: aborted\n");
    return 1;
  }
  return 0;
}
