/*****************************************************************************/
/*                                                                           */
/*                        ASM (B32P3 Assembler)                              */
/*                                                                           */
/*                   Native assembler for B32P3                              */
/*         Specifically made to assemble the output of B32CC on FPGC         */
/*                   Assembles for userBDOS (PIC mode)                       */
/*                                                                           */
/*****************************************************************************/

/* Notes:
 * - Always produces PIC (position-independent) userBDOS binaries
 * - Always adds header: jump Main, nop, .dw filesize
 * - Outputs raw 32-bit binary words directly
 * - Does not support #include, #define, or other preprocessor directives
 * - Does not support .dbb, .ddb, .dsb, .dbw, .ddw data directives
 * - Input is expected to be B32CC output (well-formed assembly)
 * - Ported from ASMPY (Python assembler)
 */

#define USER_SYSCALL
#define COMMON_STRING
#define COMMON_STDLIB
#define COMMON_CTYPE
#include "libs/user/user.h"
#include "libs/common/common.h"

/*===========================================================================*/
/*  Constants                                                                */
/*===========================================================================*/

#define MAX_LABELS       2048
#define LABEL_NAME_LEN   40
#define MAX_INSTRUCTIONS 65536
#define MAX_LINE_LEN     256
#define MAX_ADDR2REG     4096
#define MAX_ITERATIONS   32

/* Instruction opcodes (4 bits, MSB first) */
#define OP_ARITH   0x0   /* 0000 single-cycle register */
#define OP_ARITHC  0x1   /* 0001 single-cycle constant */
#define OP_ARITHMC 0x2   /* 0010 multi-cycle constant  */
#define OP_ARITHM  0x3   /* 0011 multi-cycle register  */
#define OP_RETI    0x4   /* 0100 */
#define OP_SAVPC   0x5   /* 0101 */
#define OP_BRANCH  0x6   /* 0110 */
#define OP_CCACHE  0x7   /* 0111 */
#define OP_JUMPR   0x8   /* 1000 jumpr/jumpro */
#define OP_JUMP    0x9   /* 1001 */
#define OP_POP     0xA   /* 1010 */
#define OP_PUSH    0xB   /* 1011 */
#define OP_INTID   0xC   /* 1100 */
#define OP_WRITE   0xD   /* 1101 */
#define OP_READ    0xE   /* 1110 */
#define OP_HALT    0xF   /* 1111 */

/* Single-cycle arithmetic opcodes (4 bits) */
#define ARITH_OR     0x0
#define ARITH_AND    0x1
#define ARITH_XOR    0x2
#define ARITH_ADD    0x3
#define ARITH_SUB    0x4
#define ARITH_SHIFTL 0x5
#define ARITH_SHIFTR 0x6
#define ARITH_NOT    0x7
#define ARITH_SLT    0xA
#define ARITH_SLTU   0xB
#define ARITH_LOAD   0xC
#define ARITH_LOADHI 0xD
#define ARITH_SHIFTRS 0xE

/* Multi-cycle arithmetic opcodes (4 bits) */
#define ARITHM_MULTS  0x0
#define ARITHM_MULTU  0x1
#define ARITHM_MULTFP 0x2
#define ARITHM_DIVS   0x3
#define ARITHM_DIVU   0x4
#define ARITHM_DIVFP  0x5
#define ARITHM_MODS   0x6
#define ARITHM_MODU   0x7

/* Branch opcodes (3 bits) */
#define BR_BEQ  0x0
#define BR_BGT  0x1
#define BR_BGE  0x2
#define BR_BNE  0x4
#define BR_BLT  0x5
#define BR_BLE  0x6

/* Line types */
#define LINE_INSTRUCTION 0
#define LINE_LABEL       1
#define LINE_DIRECTIVE   2
#define LINE_DATA        3
#define LINE_ADDR2REG    4  /* placeholder for PIC addr2reg */

/* Section types */
#define SECTION_CODE  0
#define SECTION_DATA  1
#define SECTION_RDATA 2
#define SECTION_BSS   3

/*===========================================================================*/
/*  Global data (allocated on heap)                                          */
/*===========================================================================*/

/* Label table */
char  (*label_names)[LABEL_NAME_LEN]; /* label_names[MAX_LABELS][LABEL_NAME_LEN] */
int   *label_addresses;               /* resolved address for each label */
int   *label_prog_indices;            /* prog_count index for each label */
int   label_count;

/* Instruction buffer: stores lines after Pass 1 as pointers into text_buf */
/* Each line is one of: instruction, .dw data, addr2reg placeholder         */
char  **line_ptrs;          /* line_ptrs[MAX_INSTRUCTIONS] */
int   *line_types;          /* LINE_INSTRUCTION, LINE_LABEL, LINE_DATA, LINE_ADDR2REG */
int   *line_sections;       /* which section each line belongs to */
int   line_count;

/* After section reordering + label removal: the "program" arrays */
char  **prog_lines;         /* prog_lines[MAX_INSTRUCTIONS] */
int   *prog_types;          /* type of each program line */
int   prog_count;

/* addr2reg tracking */
int   *a2r_indices;         /* indices in prog_lines that are addr2reg */
int   *a2r_word_sizes;      /* current estimated word count for each addr2reg */
char  (*a2r_labels)[LABEL_NAME_LEN]; /* target label for each addr2reg */
int   *a2r_registers;       /* target register number for each addr2reg */
int   a2r_count;

/* Output buffer */
unsigned int *output_words;
int   output_count;

/* Text buffer for file contents and working strings */
char  *text_buf;
int   text_buf_size;

/* Temporary line buffer */
char  line_buf[MAX_LINE_LEN];

/*===========================================================================*/
/*  Utility functions                                                        */
/*===========================================================================*/

void print_str(char *s)
{
  sys_print_str(s);
}

/* Global error flag - set by error functions, checked by main */
int has_error;

void print_int(int n)
{
  char buf[16];
  itoa(n, buf, 10);
  sys_print_str(buf);
}

void print_hex(unsigned int n)
{
  char buf[16];
  utoa(n, buf, 16, 0);
  sys_print_str("0x");
  sys_print_str(buf);
}

void error_exit(char *msg)
{
  sys_print_str("ASM Error: ");
  sys_print_str(msg);
  sys_print_str("\n");
  has_error = 1;
}

void error_exit_s(char *msg, char *arg)
{
  sys_print_str("ASM Error: ");
  sys_print_str(msg);
  sys_print_str(arg);
  sys_print_str("\n");
  has_error = 1;
}

void error_exit_i(char *msg, int arg)
{
  sys_print_str("ASM Error: ");
  sys_print_str(msg);
  print_int(arg);
  sys_print_str("\n");
  has_error = 1;
}

/*===========================================================================*/
/*  String/number helpers                                                    */
/*===========================================================================*/

/* Check if character is a letter or underscore (label start) */
int is_label_char(int c)
{
  return isalpha(c) || c == '_';
}

/* Check if string represents a register (r0-r15) */
int parse_register(char *s)
{
  if (s[0] != 'r') return -1;
  if (s[1] == 0) return -1;
  if (s[1] >= '0' && s[1] <= '9')
  {
    if (s[2] == 0) return s[1] - '0';
    if (s[2] >= '0' && s[2] <= '9' && s[3] == 0)
    {
      int val;
      val = (s[1] - '0') * 10 + (s[2] - '0');
      if (val >= 0 && val <= 15) return val;
    }
  }
  return -1;
}

/* Parse a number from string: decimal, 0x hex, 0b binary */
int parse_number(char *s, int *result)
{
  int neg;
  int val;
  int i;

  neg = 0;
  i = 0;

  if (s[0] == '-')
  {
    neg = 1;
    i = 1;
  }

  if (s[i] == '0' && (s[i+1] == 'x' || s[i+1] == 'X'))
  {
    /* Hexadecimal */
    i = i + 2;
    val = 0;
    while (s[i] != 0)
    {
      val = val << 4;
      if (s[i] >= '0' && s[i] <= '9')      val = val + (s[i] - '0');
      else if (s[i] >= 'a' && s[i] <= 'f') val = val + (s[i] - 'a' + 10);
      else if (s[i] >= 'A' && s[i] <= 'F') val = val + (s[i] - 'A' + 10);
      else return 0;
      i = i + 1;
    }
    if (neg) val = -val;
    *result = val;
    return 1;
  }
  else if (s[i] == '0' && (s[i+1] == 'b' || s[i+1] == 'B'))
  {
    /* Binary */
    i = i + 2;
    val = 0;
    while (s[i] != 0)
    {
      val = val << 1;
      if (s[i] == '1') val = val + 1;
      else if (s[i] != '0') return 0;
      i = i + 1;
    }
    if (neg) val = -val;
    *result = val;
    return 1;
  }
  else
  {
    /* Decimal */
    val = 0;
    while (s[i] != 0)
    {
      if (s[i] < '0' || s[i] > '9') return 0;
      val = val * 10 + (s[i] - '0');
      i = i + 1;
    }
    if (neg) val = -val;
    *result = val;
    return 1;
  }
}

/* Tokenize a string by spaces into tokens[]. Returns token count. */
/* Modifies the input string (inserts null terminators). */
int tokenize(char *s, char **tokens, int max_tokens)
{
  int count;
  int in_token;

  count = 0;
  in_token = 0;

  while (*s != 0)
  {
    if (*s == ' ' || *s == '\t')
    {
      if (in_token)
      {
        *s = 0;
        in_token = 0;
      }
    }
    else
    {
      if (!in_token)
      {
        if (count >= max_tokens) return count;
        tokens[count] = s;
        count = count + 1;
        in_token = 1;
      }
    }
    s = s + 1;
  }
  return count;
}

/* Copy a string safely up to max chars */
void str_copy_n(char *dest, char *src, int max)
{
  int i;
  i = 0;
  while (src[i] != 0 && i < max - 1)
  {
    dest[i] = src[i];
    i = i + 1;
  }
  dest[i] = 0;
}

/*===========================================================================*/
/*  Label table management                                                   */
/*===========================================================================*/

int find_label(char *name)
{
  int i;
  for (i = 0; i < label_count; i = i + 1)
  {
    if (strcmp(label_names[i], name) == 0)
    {
      return i;
    }
  }
  return -1;
}

int add_label(char *name, int target_prog_index)
{
  if (label_count >= MAX_LABELS)
  {
    error_exit("Too many labels");
    return -1;
  }
  if (find_label(name) >= 0)
  {
    error_exit_s("Duplicate label: ", name);
    return -1;
  }
  str_copy_n(label_names[label_count], name, LABEL_NAME_LEN);
  label_addresses[label_count] = -1; /* not yet resolved */
  label_prog_indices[label_count] = target_prog_index;
  label_count = label_count + 1;
  return label_count - 1;
}

/*===========================================================================*/
/*  File reading                                                             */
/*===========================================================================*/

/* Read entire input file into text_buf. Returns size in words (chars). */
int read_input_file(char *path)
{
  int fd;
  int fsize;
  int words_read;
  int total_read;
  int chunk;

  fd = sys_fs_open(path);
  if (fd < 0)
  {
    error_exit_s("Cannot open file: ", path);
    return -1;
  }

  fsize = sys_fs_filesize(fd);
  if (fsize <= 0)
  {
    sys_fs_close(fd);
    error_exit_s("Empty or invalid file: ", path);
    return -1;
  }

  text_buf_size = fsize + 1; /* +1 for null terminator */
  text_buf = (char *)sys_heap_alloc(text_buf_size);
  if (text_buf == 0)
  {
    sys_fs_close(fd);
    error_exit("Failed to allocate memory for input file");
    return -1;
  }

  total_read = 0;
  while (total_read < fsize)
  {
    chunk = fsize - total_read;
    if (chunk > 256) chunk = 256;
    words_read = sys_fs_read(fd, (unsigned int *)&text_buf[total_read], chunk);
    if (words_read <= 0) break;
    total_read = total_read + words_read;
  }
  text_buf[total_read] = 0;

  sys_fs_close(fd);
  return total_read;
}

/*===========================================================================*/
/*  Line reading from text buffer                                            */
/*===========================================================================*/

int text_cursor; /* current position in text_buf */

/* Read the next line from text_buf into line_buf. Strip comments and
   leading/trailing whitespace. Returns 0 if no more lines. */
int read_next_line()
{
  int out_i;
  int found_start;
  int found_comment;
  char c;

  if (text_cursor >= text_buf_size || text_buf[text_cursor] == 0)
    return 0;

  out_i = 0;
  found_start = 0;
  found_comment = 0;
  int in_quotes = 0;

  while (text_buf[text_cursor] != 0 && text_buf[text_cursor] != '\n')
  {
    c = text_buf[text_cursor];
    text_cursor = text_cursor + 1;

    if (c == '"') in_quotes = !in_quotes;
    if (c == ';' && !in_quotes) found_comment = 1;

    if (!found_comment)
    {
      if (!found_start)
      {
        if (c != ' ' && c != '\t')
        {
          found_start = 1;
          if (out_i < MAX_LINE_LEN - 1)
          {
            line_buf[out_i] = c;
            out_i = out_i + 1;
          }
        }
      }
      else
      {
        if (out_i < MAX_LINE_LEN - 1)
        {
          line_buf[out_i] = c;
          out_i = out_i + 1;
        }
      }
    }
  }

  /* Skip the newline */
  if (text_buf[text_cursor] == '\n')
    text_cursor = text_cursor + 1;

  /* Trim trailing spaces */
  while (out_i > 0 && (line_buf[out_i - 1] == ' ' || line_buf[out_i - 1] == '\t'))
    out_i = out_i - 1;

  line_buf[out_i] = 0;
  return 1;
}

/*===========================================================================*/
/*  Heap-allocated string storage                                            */
/*===========================================================================*/

char *str_pool;
int   str_pool_pos;
int   str_pool_size;

/* Store a string in the pool and return a pointer to it */
char *store_string(char *s)
{
  int len;
  char *result;

  len = strlen(s);
  if (str_pool_pos + len + 1 > str_pool_size)
  {
    error_exit("String pool exhausted");
    return 0;
  }
  result = &str_pool[str_pool_pos];
  strcpy(result, s);
  str_pool_pos = str_pool_pos + len + 1;
  return result;
}

/*===========================================================================*/
/*  Pass 1: Parse lines, collect labels, expand pseudo-instructions          */
/*===========================================================================*/

/* Expand load32 into load + loadhi. Returns number of lines added. */
int expand_load32(char *value_str, char *reg_str)
{
  int val;
  int low16;
  int high16;
  char buf[MAX_LINE_LEN];
  char num_buf[16];

  if (!parse_number(value_str, &val))
  {
    error_exit_s("Invalid number in load32: ", value_str);
    return 0;
  }

  low16 = val & 0xFFFF;
  high16 = (val >> 16) & 0xFFFF;

  /* load low16 reg */
  strcpy(buf, "load ");
  itoa(low16, num_buf, 10);
  strcat(buf, num_buf);
  strcat(buf, " ");
  strcat(buf, reg_str);

  prog_lines[prog_count] = store_string(buf);
  prog_types[prog_count] = LINE_INSTRUCTION;
  prog_count = prog_count + 1;

  /* loadhi high16 reg (only if non-zero) */
  if (high16 != 0)
  {
    strcpy(buf, "loadhi ");
    itoa(high16, num_buf, 10);
    strcat(buf, num_buf);
    strcat(buf, " ");
    strcat(buf, reg_str);

    prog_lines[prog_count] = store_string(buf);
    prog_types[prog_count] = LINE_INSTRUCTION;
    prog_count = prog_count + 1;
    return 2;
  }
  return 1;
}

/* Expand .dsw "string" into multiple .dw values */
int expand_dsw(char *rest)
{
  int i;
  int start;
  int end;
  int count;
  char buf[MAX_LINE_LEN];
  char num_buf[16];
  int char_val;

  /* Find opening quote */
  start = -1;
  end = -1;
  i = 0;
  while (rest[i] != 0)
  {
    if (rest[i] == '"')
    {
      if (start < 0) start = i + 1;
      else end = i;
    }
    i = i + 1;
  }

  if (start < 0 || end < 0)
  {
    error_exit("Unterminated string in .dsw");
    return 0;
  }

  count = 0;
  i = start;
  while (i < end)
  {
    if (rest[i] == '\\' && i + 1 < end)
    {
      i = i + 1;
      if (rest[i] == 'n') char_val = 10;
      else if (rest[i] == 'r') char_val = 13;
      else if (rest[i] == 't') char_val = 9;
      else if (rest[i] == '\\') char_val = 92;
      else if (rest[i] == '"') char_val = 34;
      else if (rest[i] == '0')
      {
        /* Octal escape */
        int octal_val;
        int octal_count;
        octal_val = 0;
        octal_count = 0;
        while (i < end && rest[i] >= '0' && rest[i] <= '7' && octal_count < 3)
        {
          octal_val = octal_val * 8 + (rest[i] - '0');
          i = i + 1;
          octal_count = octal_count + 1;
        }
        char_val = octal_val;
        i = i - 1; /* will be incremented at end of loop */
      }
      else char_val = rest[i]; /* unknown escape: literal */
      i = i + 1;
    }
    else
    {
      char_val = rest[i];
      i = i + 1;
    }

    strcpy(buf, ".dw ");
    itoa(char_val, num_buf, 10);
    strcat(buf, num_buf);

    prog_lines[prog_count] = store_string(buf);
    prog_types[prog_count] = LINE_DATA;
    prog_count = prog_count + 1;
    count = count + 1;
  }

  return count;
}

/* Expand .dw with multiple values into separate .dw lines */
int expand_dw_multi(char *rest)
{
  char *tokens[32];
  int n_tokens;
  int i;
  char buf[MAX_LINE_LEN];
  char rest_copy[MAX_LINE_LEN];

  str_copy_n(rest_copy, rest, MAX_LINE_LEN);
  n_tokens = tokenize(rest_copy, tokens, 32);

  if (n_tokens <= 0)
  {
    error_exit("Empty .dw directive");
    return 0;
  }

  for (i = 0; i < n_tokens; i = i + 1)
  {
    strcpy(buf, ".dw ");
    strcat(buf, tokens[i]);

    prog_lines[prog_count] = store_string(buf);
    prog_types[prog_count] = LINE_DATA;
    prog_count = prog_count + 1;
  }

  return n_tokens;
}

/*===========================================================================*/
/*  Pass 1 main function                                                     */
/*===========================================================================*/

/* Temporary storage for section separation */
char  **section_lines[4];
int   *section_types[4];
int   section_counts[4];
int   section_max;

int current_section;

/* Pending labels - labels that need to point to the next instruction */
char  pending_labels[32][LABEL_NAME_LEN];
int   pending_label_section[32]; /* which section each pending label goes to */
int   pending_label_count;

void init_sections()
{
  int i;
  section_max = MAX_INSTRUCTIONS / 4;
  for (i = 0; i < 4; i = i + 1)
  {
    section_lines[i] = (char **)sys_heap_alloc(section_max);
    section_types[i] = (int *)sys_heap_alloc(section_max);
    section_counts[i] = 0;
  }
  current_section = SECTION_CODE;
  pending_label_count = 0;
}

void add_to_section(int section, char *line, int type)
{
  int idx;
  idx = section_counts[section];
  if (idx >= section_max)
  {
    error_exit("Section overflow");
    return;
  }
  section_lines[section][idx] = line;
  section_types[section][idx] = type;
  section_counts[section] = idx + 1;
}

/* Flush pending labels into the given section */
void flush_pending_labels(int section)
{
  int i;
  for (i = 0; i < pending_label_count; i = i + 1)
  {
    add_to_section(section, store_string(pending_labels[i]), LINE_LABEL);
  }
  pending_label_count = 0;
}

void pass1()
{
  char *tokens[16];
  int n_tokens;
  char line_copy[MAX_LINE_LEN];
  int len;

  text_cursor = 0;

  while (read_next_line())
  {
    /* Skip empty lines */
    if (line_buf[0] == 0) continue;

    /* Skip preprocessor directives */
    if (line_buf[0] == '#') continue;

    len = strlen(line_buf);

    /* Check for section directives */
    if (line_buf[0] == '.' && !isdigit(line_buf[1]))
    {
      /* Check if it's a data directive or section directive */
      if (strncmp(line_buf, ".code", 5) == 0)
      {
        /* Flush pending labels to current section before switching */
        flush_pending_labels(current_section);
        current_section = SECTION_CODE;
        continue;
      }
      else if (strncmp(line_buf, ".data", 5) == 0)
      {
        flush_pending_labels(current_section);
        current_section = SECTION_DATA;
        continue;
      }
      else if (strncmp(line_buf, ".rdata", 6) == 0)
      {
        flush_pending_labels(current_section);
        current_section = SECTION_RDATA;
        continue;
      }
      else if (strncmp(line_buf, ".bss", 4) == 0)
      {
        flush_pending_labels(current_section);
        current_section = SECTION_BSS;
        continue;
      }
      else if (strncmp(line_buf, ".dw ", 4) == 0)
      {
        /* Data word - flush pending labels first */
        flush_pending_labels(current_section);
        /* Store directly */
        add_to_section(current_section, store_string(line_buf), LINE_DATA);
        continue;
      }
      else if (strncmp(line_buf, ".dsw ", 5) == 0)
      {
        /* Data string word - this will be expanded later */
        flush_pending_labels(current_section);
        add_to_section(current_section, store_string(line_buf), LINE_DATA);
        continue;
      }
    }

    /* Check for label (ends with ':') */
    if (len > 1 && line_buf[len - 1] == ':')
    {
      str_copy_n(pending_labels[pending_label_count], line_buf, len); /* copy without ':' */
      pending_labels[pending_label_count][len - 1] = 0;
      pending_label_section[pending_label_count] = current_section;
      pending_label_count = pending_label_count + 1;
      continue;
    }

    /* It's an instruction line */
    flush_pending_labels(current_section);
    add_to_section(current_section, store_string(line_buf), LINE_INSTRUCTION);
  }

  /* Flush any remaining pending labels */
  flush_pending_labels(current_section);
}

/*===========================================================================*/
/*  Section merging and instruction expansion                                */
/*===========================================================================*/

void merge_sections_and_expand()
{
  int s;
  int i;
  char *tokens[16];
  int n_tokens;
  char line_copy[MAX_LINE_LEN];
  char *line;
  int type;

  prog_count = 0;
  a2r_count = 0;

  /* Add header: jump Main, nop, .dw 0 */
  prog_lines[prog_count] = store_string("jump Main");
  prog_types[prog_count] = LINE_INSTRUCTION;
  prog_count = prog_count + 1;

  prog_lines[prog_count] = store_string("nop");
  prog_types[prog_count] = LINE_INSTRUCTION;
  prog_count = prog_count + 1;

  prog_lines[prog_count] = store_string(".dw 0");
  prog_types[prog_count] = LINE_DATA;
  prog_count = prog_count + 1;

  /* Merge sections in order: code, data, rdata, bss */
  for (s = 0; s < 4; s = s + 1)
  {
    for (i = 0; i < section_counts[s]; i = i + 1)
    {
      line = section_lines[s][i];
      type = section_types[s][i];

      if (type == LINE_LABEL)
      {
        /* Labels: add to label table, pointing to next prog_count */
        add_label(line, prog_count);
        continue;
      }

      if (type == LINE_DATA)
      {
        /* Handle .dsw expansion */
        if (strncmp(line, ".dsw ", 5) == 0)
        {
          expand_dsw(line + 4); /* skip ".dsw" */
          continue;
        }

        /* Handle multi-value .dw */
        if (strncmp(line, ".dw ", 4) == 0)
        {
          char *rest;
          rest = line + 4;
          /* Check if there are multiple values (spaces in rest) */
          if (strchr(rest, ' ') != 0)
          {
            expand_dw_multi(rest);
          }
          else
          {
            prog_lines[prog_count] = line;
            prog_types[prog_count] = LINE_DATA;
            prog_count = prog_count + 1;
          }
          continue;
        }
      }

      /* Instruction: check for pseudo-instructions */
      str_copy_n(line_copy, line, MAX_LINE_LEN);
      n_tokens = tokenize(line_copy, tokens, 16);

      if (n_tokens <= 0) continue;

      if (strcmp(tokens[0], "load32") == 0)
      {
        if (n_tokens != 3)
        {
          error_exit("load32 requires 2 arguments");
          continue;
        }
        /* Re-parse the original to get the tokens (since tokenize modified line_copy) */
        str_copy_n(line_copy, line, MAX_LINE_LEN);
        n_tokens = tokenize(line_copy, tokens, 16);
        expand_load32(tokens[1], tokens[2]);
        continue;
      }

      if (strcmp(tokens[0], "addr2reg") == 0)
      {
        if (n_tokens != 3)
        {
          error_exit("addr2reg requires 2 arguments");
          continue;
        }
        /* Store as addr2reg placeholder for PIC processing */
        /* Re-parse from original */
        str_copy_n(line_copy, line, MAX_LINE_LEN);
        n_tokens = tokenize(line_copy, tokens, 16);

        prog_lines[prog_count] = line;
        prog_types[prog_count] = LINE_ADDR2REG;

        /* Record addr2reg info */
        a2r_indices[a2r_count] = prog_count;
        str_copy_n(a2r_labels[a2r_count], tokens[1], LABEL_NAME_LEN);
        a2r_registers[a2r_count] = parse_register(tokens[2]);
        if (a2r_registers[a2r_count] < 0)
        {
          error_exit_s("Invalid register in addr2reg: ", tokens[2]);
        }
        a2r_word_sizes[a2r_count] = 2; /* initial estimate: savpc + one add */
        a2r_count = a2r_count + 1;

        prog_count = prog_count + 1;
        continue;
      }

      /* Normal instruction - store as-is */
      prog_lines[prog_count] = line;
      prog_types[prog_count] = LINE_INSTRUCTION;
      prog_count = prog_count + 1;
    }
  }
}

/*===========================================================================*/
/*  Label address assignment                                                 */
/*  Must skip label-only entries (they're removed), account for addr2reg     */
/*  word sizes.                                                              */
/*===========================================================================*/

/* Assign label addresses. A label points to prog_count index, but the
   actual word address depends on addr2reg expansion sizes. */

/* Since we removed label lines from prog_lines in merge_sections_and_expand,
   labels are stored in label_names[] with address = -1 (unresolved).
   We now need to figure out which prog_count index each label corresponds to.
   
   The trick: in merge_sections_and_expand, when we encounter a label,
   its "next instruction" is the next prog_lines entry we add.
   So we need to record the prog_count at the time of each label add. */

/* Actually let me restructure: instead of removing labels from prog, we store
   the mapping at label creation time. */

/* This is called after merge_sections_and_expand to resolve actual addresses */
void resolve_label_addresses()
{
  int i;
  int addr;
  int a2r_idx;
  int prog_idx;

  /* Compute word address for each prog_count entry */
  addr = 0;
  for (i = 0; i < prog_count; i = i + 1)
  {
    /* Check if this index is an addr2reg - if so, it takes a2r_word_sizes words */
    a2r_idx = -1;
    {
      int j;
      for (j = 0; j < a2r_count; j = j + 1)
      {
        if (a2r_indices[j] == i)
        {
          a2r_idx = j;
          break;
        }
      }
    }

    /* Set address for any labels pointing to this prog index */
    {
      int j;
      for (j = 0; j < label_count; j = j + 1)
      {
        if (label_prog_indices[j] == i)
        {
          label_addresses[j] = addr;
        }
      }
    }

    if (a2r_idx >= 0)
    {
      addr = addr + a2r_word_sizes[a2r_idx];
    }
    else
    {
      addr = addr + 1;
    }
  }

  /* Handle labels at the very end (pointing past last instruction) */
  {
    int j;
    for (j = 0; j < label_count; j = j + 1)
    {
      if (label_prog_indices[j] == prog_count)
      {
        label_addresses[j] = addr;
      }
    }
  }
}

/* Get the word address of a given prog index, considering addr2reg sizes */
int get_prog_address(int prog_idx)
{
  int i;
  int addr;
  int a2r_idx;

  addr = 0;
  for (i = 0; i < prog_idx; i = i + 1)
  {
    a2r_idx = -1;
    {
      int j;
      for (j = 0; j < a2r_count; j = j + 1)
      {
        if (a2r_indices[j] == i)
        {
          a2r_idx = j;
          break;
        }
      }
    }

    if (a2r_idx >= 0)
    {
      addr = addr + a2r_word_sizes[a2r_idx];
    }
    else
    {
      addr = addr + 1;
    }
  }
  return addr;
}

/*===========================================================================*/
/*  PIC: signed 16-bit chunk splitting                                       */
/*===========================================================================*/

int chunk_buf[32]; /* max 32 chunks */

/* Split a value into signed 16-bit chunks. Returns count. */
int split_signed_16bit_chunks(int value)
{
  int count;
  int remaining;
  int chunk;

  count = 0;
  remaining = value;
  while (remaining != 0)
  {
    if (remaining > 32767) chunk = 32767;
    else if (remaining < -32768) chunk = -32768;
    else chunk = remaining;

    chunk_buf[count] = chunk;
    count = count + 1;
    remaining = remaining - chunk;
  }

  /* If value is 0, we still need at least one add with 0 */
  if (count == 0)
  {
    chunk_buf[0] = 0;
    count = 1;
  }

  return count;
}

/*===========================================================================*/
/*  PIC Stabilization                                                        */
/*===========================================================================*/

void pic_stabilize()
{
  int iteration;
  int changed;
  int i;
  int prog_idx;
  int target_label_idx;
  int target_addr;
  int current_addr;
  int offset;
  int new_size;

  for (iteration = 0; iteration < MAX_ITERATIONS; iteration = iteration + 1)
  {
    /* Resolve label addresses with current word sizes */
    resolve_label_addresses();

    changed = 0;
    for (i = 0; i < a2r_count; i = i + 1)
    {
      prog_idx = a2r_indices[i];
      current_addr = get_prog_address(prog_idx);

      target_label_idx = find_label(a2r_labels[i]);
      if (target_label_idx < 0)
      {
        error_exit_s("Unresolved label in addr2reg: ", a2r_labels[i]);
        return;
      }
      target_addr = label_addresses[target_label_idx];
      offset = target_addr - current_addr;

      new_size = 1 + split_signed_16bit_chunks(offset); /* 1 for savpc + N for adds */

      if (new_size != a2r_word_sizes[i])
      {
        a2r_word_sizes[i] = new_size;
        changed = 1;
      }
    }

    if (!changed) return;
  }

  error_exit("PIC addr2reg stabilization did not converge");
}

/*===========================================================================*/
/*  Instruction encoding                                                     */
/*===========================================================================*/

unsigned int encode_instruction(char *line, int current_addr)
{
  char line_copy[MAX_LINE_LEN];
  char *tokens[16];
  int n_tokens;
  char *mnemonic;
  unsigned int word;
  int reg_a;
  int reg_b;
  int reg_d;
  int val;
  int arith_op;
  int br_op;
  int is_signed;

  str_copy_n(line_copy, line, MAX_LINE_LEN);
  n_tokens = tokenize(line_copy, tokens, 16);

  if (n_tokens <= 0)
  {
    error_exit("Empty instruction");
    return 0;
  }

  mnemonic = tokens[0];

  /* ----- Control operations ----- */

  if (strcmp(mnemonic, "halt") == 0)
  {
    return 0xFFFFFFFF;
  }

  if (strcmp(mnemonic, "nop") == 0)
  {
    return 0x00000000;
  }

  if (strcmp(mnemonic, "savpc") == 0)
  {
    reg_d = parse_register(tokens[1]);
    return (OP_SAVPC << 28) | (reg_d & 0xF);
  }

  if (strcmp(mnemonic, "ccache") == 0)
  {
    return (OP_CCACHE << 28);
  }

  if (strcmp(mnemonic, "reti") == 0)
  {
    return (OP_RETI << 28);
  }

  if (strcmp(mnemonic, "readintid") == 0)
  {
    reg_d = parse_register(tokens[1]);
    return (OP_INTID << 28) | (reg_d & 0xF);
  }

  /* ----- Memory operations ----- */

  if (strcmp(mnemonic, "read") == 0)
  {
    /* read const16 areg dreg */
    parse_number(tokens[1], &val);
    reg_a = parse_register(tokens[2]);
    reg_d = parse_register(tokens[3]);
    return (OP_READ << 28) | ((val & 0xFFFF) << 12) | ((reg_a & 0xF) << 8) | (reg_d & 0xF);
  }

  if (strcmp(mnemonic, "write") == 0)
  {
    /* write const16 areg breg */
    parse_number(tokens[1], &val);
    reg_a = parse_register(tokens[2]);
    reg_b = parse_register(tokens[3]);
    return (OP_WRITE << 28) | ((val & 0xFFFF) << 12) | ((reg_a & 0xF) << 8) | ((reg_b & 0xF) << 4);
  }

  if (strcmp(mnemonic, "push") == 0)
  {
    reg_b = parse_register(tokens[1]);
    return (OP_PUSH << 28) | ((reg_b & 0xF) << 4);
  }

  if (strcmp(mnemonic, "pop") == 0)
  {
    reg_d = parse_register(tokens[1]);
    return (OP_POP << 28) | (reg_d & 0xF);
  }

  /* ----- Jump operations ----- */

  if (strcmp(mnemonic, "jump") == 0)
  {
    /* In PIC mode, jump with label is rewritten to jumpo.
       But we may still have jump with a numeric address (jump Main rewritten to jumpo).
       Or jump in header before PIC rewrite.
       Actually, by the time we encode, all jump-to-label should be rewritten.
       Handle jump with label (resolve it as PIC offset) or jump with number. */

    if (n_tokens != 2)
    {
      error_exit("jump requires 1 argument");
      return 0;
    }

    /* Check if it's a label */
    if (is_label_char(tokens[1][0]))
    {
      /* PIC: convert to jumpo with offset */
      int label_idx;
      int target;
      int offset;
      label_idx = find_label(tokens[1]);
      if (label_idx < 0)
      {
        error_exit_s("Unresolved label: ", tokens[1]);
        return 0;
      }
      target = label_addresses[label_idx];
      offset = target - current_addr;
      return (OP_JUMP << 28) | ((offset & 0x7FFFFFF) << 1) | 1;
    }
    else
    {
      /* Numeric absolute address */
      parse_number(tokens[1], &val);
      return (OP_JUMP << 28) | ((val & 0x7FFFFFF) << 1) | 0;
    }
  }

  if (strcmp(mnemonic, "jumpo") == 0)
  {
    parse_number(tokens[1], &val);
    return (OP_JUMP << 28) | ((val & 0x7FFFFFF) << 1) | 1;
  }

  if (strcmp(mnemonic, "jumpr") == 0)
  {
    /* jumpr const16 breg */
    parse_number(tokens[1], &val);
    reg_b = parse_register(tokens[2]);
    return (OP_JUMPR << 28) | ((val & 0xFFFF) << 12) | ((reg_b & 0xF) << 4) | 0;
  }

  if (strcmp(mnemonic, "jumpro") == 0)
  {
    /* jumpro const16 breg */
    parse_number(tokens[1], &val);
    reg_b = parse_register(tokens[2]);
    return (OP_JUMPR << 28) | ((val & 0xFFFF) << 12) | ((reg_b & 0xF) << 4) | 1;
  }

  /* ----- Branch operations ----- */

  br_op = -1;
  is_signed = 0;

  if (strcmp(mnemonic, "beq") == 0) { br_op = BR_BEQ; is_signed = 0; }
  else if (strcmp(mnemonic, "bne") == 0)  { br_op = BR_BNE; is_signed = 0; }
  else if (strcmp(mnemonic, "bgt") == 0)  { br_op = BR_BGT; is_signed = 0; }
  else if (strcmp(mnemonic, "bge") == 0)  { br_op = BR_BGE; is_signed = 0; }
  else if (strcmp(mnemonic, "blt") == 0)  { br_op = BR_BLT; is_signed = 0; }
  else if (strcmp(mnemonic, "ble") == 0)  { br_op = BR_BLE; is_signed = 0; }
  else if (strcmp(mnemonic, "bgts") == 0) { br_op = BR_BGT; is_signed = 1; }
  else if (strcmp(mnemonic, "bges") == 0) { br_op = BR_BGE; is_signed = 1; }
  else if (strcmp(mnemonic, "blts") == 0) { br_op = BR_BLT; is_signed = 1; }
  else if (strcmp(mnemonic, "bles") == 0) { br_op = BR_BLE; is_signed = 1; }

  if (br_op >= 0)
  {
    /* branch areg breg offset_or_label */
    reg_a = parse_register(tokens[1]);
    reg_b = parse_register(tokens[2]);

    if (is_label_char(tokens[3][0]))
    {
      /* Label: compute relative offset */
      int label_idx;
      label_idx = find_label(tokens[3]);
      if (label_idx < 0)
      {
        error_exit_s("Unresolved branch label: ", tokens[3]);
        return 0;
      }
      val = label_addresses[label_idx] - current_addr;
    }
    else
    {
      parse_number(tokens[3], &val);
    }

    return (OP_BRANCH << 28) | ((val & 0xFFFF) << 12) | ((reg_a & 0xF) << 8) | ((reg_b & 0xF) << 4) | ((br_op & 0x7) << 1) | (is_signed & 1);
  }

  /* ----- Single-cycle arithmetic operations ----- */

  arith_op = -1;
  if (strcmp(mnemonic, "or") == 0)       arith_op = ARITH_OR;
  else if (strcmp(mnemonic, "and") == 0) arith_op = ARITH_AND;
  else if (strcmp(mnemonic, "xor") == 0) arith_op = ARITH_XOR;
  else if (strcmp(mnemonic, "add") == 0) arith_op = ARITH_ADD;
  else if (strcmp(mnemonic, "sub") == 0) arith_op = ARITH_SUB;
  else if (strcmp(mnemonic, "shiftl") == 0)  arith_op = ARITH_SHIFTL;
  else if (strcmp(mnemonic, "shiftr") == 0)  arith_op = ARITH_SHIFTR;
  else if (strcmp(mnemonic, "shiftrs") == 0) arith_op = ARITH_SHIFTRS;
  else if (strcmp(mnemonic, "slt") == 0)     arith_op = ARITH_SLT;
  else if (strcmp(mnemonic, "sltu") == 0)    arith_op = ARITH_SLTU;

  if (arith_op >= 0)
  {
    /* arith areg breg/const dreg */
    reg_a = parse_register(tokens[1]);

    /* Check if second argument is register or constant */
    reg_b = parse_register(tokens[2]);
    if (reg_b >= 0)
    {
      /* Register form */
      reg_d = parse_register(tokens[3]);
      return (OP_ARITH << 28) | ((arith_op & 0xF) << 24) | ((reg_a & 0xF) << 8) | ((reg_b & 0xF) << 4) | (reg_d & 0xF);
    }
    else
    {
      /* Constant form */
      parse_number(tokens[2], &val);
      reg_d = parse_register(tokens[3]);
      return (OP_ARITHC << 28) | ((arith_op & 0xF) << 24) | ((val & 0xFFFF) << 8) | ((reg_a & 0xF) << 4) | (reg_d & 0xF);
    }
  }

  /* not areg dreg */
  if (strcmp(mnemonic, "not") == 0)
  {
    reg_a = parse_register(tokens[1]);
    reg_d = parse_register(tokens[2]);
    return (OP_ARITH << 28) | (ARITH_NOT << 24) | ((reg_a & 0xF) << 8) | (reg_d & 0xF);
  }

  /* load const16 reg */
  if (strcmp(mnemonic, "load") == 0)
  {
    /* load can have a label or number as first argument */
    reg_d = parse_register(tokens[2]);

    if (is_label_char(tokens[1][0]))
    {
      int label_idx;
      label_idx = find_label(tokens[1]);
      if (label_idx < 0)
      {
        error_exit_s("Unresolved label in load: ", tokens[1]);
        return 0;
      }
      val = label_addresses[label_idx] & 0xFFFF;
    }
    else
    {
      parse_number(tokens[1], &val);
    }

    return (OP_ARITHC << 28) | (ARITH_LOAD << 24) | ((val & 0xFFFF) << 8) | ((reg_d & 0xF) << 4) | (reg_d & 0xF);
  }

  /* loadhi const16 reg */
  if (strcmp(mnemonic, "loadhi") == 0)
  {
    reg_d = parse_register(tokens[2]);

    if (is_label_char(tokens[1][0]))
    {
      int label_idx;
      label_idx = find_label(tokens[1]);
      if (label_idx < 0)
      {
        error_exit_s("Unresolved label in loadhi: ", tokens[1]);
        return 0;
      }
      val = (label_addresses[label_idx] >> 16) & 0xFFFF;
    }
    else
    {
      parse_number(tokens[1], &val);
    }

    return (OP_ARITHC << 28) | (ARITH_LOADHI << 24) | ((val & 0xFFFF) << 8) | ((reg_d & 0xF) << 4) | (reg_d & 0xF);
  }

  /* ----- Multi-cycle arithmetic operations ----- */

  arith_op = -1;
  if (strcmp(mnemonic, "mults") == 0)  arith_op = ARITHM_MULTS;
  else if (strcmp(mnemonic, "multu") == 0)  arith_op = ARITHM_MULTU;
  else if (strcmp(mnemonic, "multfp") == 0) arith_op = ARITHM_MULTFP;
  else if (strcmp(mnemonic, "divs") == 0)   arith_op = ARITHM_DIVS;
  else if (strcmp(mnemonic, "divu") == 0)   arith_op = ARITHM_DIVU;
  else if (strcmp(mnemonic, "divfp") == 0)  arith_op = ARITHM_DIVFP;
  else if (strcmp(mnemonic, "mods") == 0)   arith_op = ARITHM_MODS;
  else if (strcmp(mnemonic, "modu") == 0)   arith_op = ARITHM_MODU;

  if (arith_op >= 0)
  {
    /* Multi-cycle: areg breg/const dreg */
    reg_a = parse_register(tokens[1]);
    reg_b = parse_register(tokens[2]);

    if (reg_b >= 0)
    {
      /* Register form */
      reg_d = parse_register(tokens[3]);
      return (OP_ARITHMC << 28) | ((arith_op & 0xF) << 24) | ((reg_a & 0xF) << 8) | ((reg_b & 0xF) << 4) | (reg_d & 0xF);
    }
    else
    {
      /* Constant form */
      parse_number(tokens[2], &val);
      reg_d = parse_register(tokens[3]);
      return (OP_ARITHM << 28) | ((arith_op & 0xF) << 24) | ((val & 0xFFFF) << 8) | ((reg_a & 0xF) << 4) | (reg_d & 0xF);
    }
  }

  error_exit_s("Unknown instruction: ", mnemonic);
  return 0;
}

/*===========================================================================*/
/*  Encode .dw data                                                          */
/*===========================================================================*/

unsigned int encode_data(char *line)
{
  char line_copy[MAX_LINE_LEN];
  char *tokens[8];
  int n_tokens;
  int val;

  str_copy_n(line_copy, line, MAX_LINE_LEN);
  n_tokens = tokenize(line_copy, tokens, 8);

  if (n_tokens < 2)
  {
    error_exit_s("Invalid .dw: ", line);
    return 0;
  }

  /* .dw can have a label or number */
  if (is_label_char(tokens[1][0]))
  {
    int label_idx;
    label_idx = find_label(tokens[1]);
    if (label_idx < 0)
    {
      error_exit_s("Unresolved label in .dw: ", tokens[1]);
      return 0;
    }
    return (unsigned int)label_addresses[label_idx];
  }

  if (!parse_number(tokens[1], &val))
  {
    error_exit_s("Invalid number in .dw: ", tokens[1]);
    return 0;
  }
  return (unsigned int)val;
}

/*===========================================================================*/
/*  Pass 2: Generate binary output                                           */
/*===========================================================================*/

void pass2()
{
  int i;
  int current_addr;
  int j;
  int a2r_idx;

  output_count = 0;
  current_addr = 0;

  for (i = 0; i < prog_count; i = i + 1)
  {
    /* Check if this is an addr2reg line */
    a2r_idx = -1;
    for (j = 0; j < a2r_count; j = j + 1)
    {
      if (a2r_indices[j] == i)
      {
        a2r_idx = j;
        break;
      }
    }

    if (a2r_idx >= 0)
    {
      /* Emit PIC addr2reg: savpc reg, then add reg chunk reg for each chunk */
      int reg;
      int target_label_idx;
      int target_addr;
      int offset;
      int n_chunks;
      int k;

      reg = a2r_registers[a2r_idx];
      target_label_idx = find_label(a2r_labels[a2r_idx]);
      target_addr = label_addresses[target_label_idx];
      offset = target_addr - current_addr;
      n_chunks = split_signed_16bit_chunks(offset);

      /* savpc reg */
      output_words[output_count] = (OP_SAVPC << 28) | (reg & 0xF);
      output_count = output_count + 1;

      /* add reg chunk reg for each chunk */
      for (k = 0; k < n_chunks; k = k + 1)
      {
        int chunk_val;
        chunk_val = chunk_buf[k];
        output_words[output_count] = (OP_ARITHC << 28) | (ARITH_ADD << 24) | ((chunk_val & 0xFFFF) << 8) | ((reg & 0xF) << 4) | (reg & 0xF);
        output_count = output_count + 1;
      }

      current_addr = current_addr + a2r_word_sizes[a2r_idx];
    }
    else if (prog_types[i] == LINE_DATA)
    {
      output_words[output_count] = encode_data(prog_lines[i]);
      output_count = output_count + 1;
      current_addr = current_addr + 1;
    }
    else
    {
      /* Normal instruction */
      output_words[output_count] = encode_instruction(prog_lines[i], current_addr);
      output_count = output_count + 1;
      current_addr = current_addr + 1;
    }
  }

  /* Update header word 2 (.dw filesize) with total output count */
  if (output_count >= 3)
  {
    output_words[2] = (unsigned int)output_count;
  }
}

/*===========================================================================*/
/*  Output writing                                                           */
/*===========================================================================*/

int write_output_file(char *path)
{
  int fd;
  int written;
  int total_written;
  int chunk;
  int remaining;

  /* Delete and recreate to ensure clean file */
  sys_fs_delete(path);
  sys_fs_create(path);

  fd = sys_fs_open(path);
  if (fd < 0)
  {
    error_exit_s("Cannot create output file: ", path);
    return -1;
  }

  total_written = 0;
  remaining = output_count;
  while (remaining > 0)
  {
    chunk = remaining;
    if (chunk > 256) chunk = 256;
    written = sys_fs_write(fd, &output_words[total_written], chunk);
    if (written <= 0)
    {
      error_exit("Write failed");
      sys_fs_close(fd);
      return -1;
    }
    total_written = total_written + written;
    remaining = remaining - written;
  }

  sys_fs_close(fd);
  return total_written;
}

/*===========================================================================*/
/*  Memory allocation                                                        */
/*===========================================================================*/

void allocate_buffers()
{
  label_names = (char (*)[LABEL_NAME_LEN])sys_heap_alloc(MAX_LABELS * LABEL_NAME_LEN);
  label_addresses = (int *)sys_heap_alloc(MAX_LABELS);
  label_prog_indices = (int *)sys_heap_alloc(MAX_LABELS);

  prog_lines = (char **)sys_heap_alloc(MAX_INSTRUCTIONS);
  prog_types = (int *)sys_heap_alloc(MAX_INSTRUCTIONS);

  a2r_indices = (int *)sys_heap_alloc(MAX_ADDR2REG);
  a2r_word_sizes = (int *)sys_heap_alloc(MAX_ADDR2REG);
  a2r_labels = (char (*)[LABEL_NAME_LEN])sys_heap_alloc(MAX_ADDR2REG * LABEL_NAME_LEN);
  a2r_registers = (int *)sys_heap_alloc(MAX_ADDR2REG);

  output_words = (unsigned int *)sys_heap_alloc(MAX_INSTRUCTIONS);

  /* String pool for all stored string copies (2 MB) */
  str_pool_size = 512 * 1024;
  str_pool = (char *)sys_heap_alloc(str_pool_size);
  str_pool_pos = 0;
}

/*===========================================================================*/
/*  Main                                                                     */
/*===========================================================================*/

int main()
{
  int argc;
  char **argv;
  char *input_path;
  char *output_path;
  char *cwd;
  char abs_input[256];
  char abs_output[256];
  int file_size;
  int i;

  argc = sys_shell_argc();
  argv = sys_shell_argv();

  if (argc < 3)
  {
    print_str("Usage: asm <input.asm> <output.bin>\n");
    return 1;
  }

  input_path = argv[1];
  output_path = argv[2];
  cwd = sys_shell_getcwd();

  /* Build absolute paths */
  if (input_path[0] != '/')
  {
    strcpy(abs_input, cwd);
    strcat(abs_input, "/");
    strcat(abs_input, input_path);
  }
  else
  {
    strcpy(abs_input, input_path);
  }

  if (output_path[0] != '/')
  {
    strcpy(abs_output, cwd);
    strcat(abs_output, "/");
    strcat(abs_output, output_path);
  }
  else
  {
    strcpy(abs_output, output_path);
  }

  /* Allocate all buffers */
  allocate_buffers();

  /* Initialize globals */
  has_error = 0;
  label_count = 0;
  line_count = 0;
  prog_count = 0;
  a2r_count = 0;
  output_count = 0;

  /* Read the input file */
  file_size = read_input_file(abs_input);
  if (file_size < 0) return 1;

  print_str("Read ");
  print_int(file_size);
  print_str(" words from ");
  print_str(abs_input);
  print_str("\n");

  /* Initialize section buffers */
  init_sections();

  /* Pass 1: parse lines into sections, collect labels */
  pass1();
  if (has_error) { print_str("Aborted due to errors in Pass 1\n"); return 1; }

  print_str("Pass 1: ");
  for (i = 0; i < 4; i = i + 1)
  {
    print_int(section_counts[i]);
    print_str(" ");
  }
  print_str("lines in code/data/rdata/bss\n");

  /* Merge sections and expand pseudo-instructions */
  merge_sections_and_expand();
  if (has_error) { print_str("Aborted due to errors in merge/expand\n"); return 1; }

  print_str("After merge: ");
  print_int(prog_count);
  print_str(" program lines, ");
  print_int(label_count);
  print_str(" labels, ");
  print_int(a2r_count);
  print_str(" addr2reg\n");

  /* PIC stabilization */
  if (a2r_count > 0)
  {
    pic_stabilize();
    if (has_error) { print_str("Aborted due to PIC errors\n"); return 1; }
    print_str("PIC stabilized\n");
  }

  /* Final label resolution */
  resolve_label_addresses();

  /* Pass 2: encode all instructions */
  pass2();
  if (has_error) { print_str("Aborted due to encoding errors\n"); return 1; }

  print_str("Pass 2: ");
  print_int(output_count);
  print_str(" output words\n");

  /* Write output */
  write_output_file(abs_output);

  print_str("Written to ");
  print_str(abs_output);
  print_str("\n");

  return 0;
}
