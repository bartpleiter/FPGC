// edit.c — Text editor for FPGC BDOS
// A nano-like terminal text editor using a gap buffer.
// Usage: edit <filename>
//
// shell-terminal-v2 port: rendering goes through ANSI escapes on fd 1
// (so the editor honours redirection and lives inside the libterm v2
// cell model), and input comes from /dev/tty in raw blocking mode via
// sys_tty_open_raw / sys_tty_event_read. Alternate screen is entered
// with \x1b[?1049h on startup and left on exit so the shell view is
// restored.

#include <syscall.h>

// ----------------------------------------------------------------------
// ANSI helpers
// ----------------------------------------------------------------------

static int tty_fd = -1;          /* /dev/tty in raw mode (input) */
static int last_palette = -1;    /* last SGR palette emitted (-1 = unknown) */

static int ansi_strlen(const char *s)
{
    int n = 0;
    while (s[n] != 0) n++;
    return n;
}

static void ansi_write(const char *s)
{
    sys_write(1, (char *)s, ansi_strlen(s));
}

static void ansi_emit_uint(int v, char *dst, int *pos)
{
    char tmp[12];
    int n = 0;
    if (v == 0) { dst[(*pos)++] = '0'; return; }
    while (v > 0) { tmp[n++] = (char)('0' + (v % 10)); v /= 10; }
    while (n > 0) { dst[(*pos)++] = tmp[--n]; }
}

/* Move cursor to (x, y) in 0-based coordinates. */
static void ansi_goto(int x, int y)
{
    char buf[16];
    int n = 0;
    buf[n++] = 0x1B; buf[n++] = '[';
    ansi_emit_uint(y + 1, buf, &n);
    buf[n++] = ';';
    ansi_emit_uint(x + 1, buf, &n);
    buf[n++] = 'H';
    sys_write(1, buf, n);
}

/* Map an 8-bit palette index to an SGR sequence:
 *   low nibble  = fg (0..7) plus optional bold (bit 3)
 *   high nibble = bg (0..7); bit 7 ignored, no SGR support for it
 * Always emits a leading 0 so prior state is cleared. */
static void ansi_set_palette(int pal)
{
    int fg, bg, bold;
    char buf[24];
    int n;

    if (pal == last_palette) return;
    last_palette = pal;

    fg   = pal & 0x07;
    bold = (pal & 0x08) ? 1 : 0;
    bg   = (pal >> 4) & 0x07;

    n = 0;
    buf[n++] = 0x1B; buf[n++] = '['; buf[n++] = '0';
    if (bold)  { buf[n++] = ';'; buf[n++] = '1'; }
    buf[n++] = ';'; buf[n++] = '4'; buf[n++] = (char)('0' + bg);
    buf[n++] = ';'; buf[n++] = '3'; buf[n++] = (char)('0' + fg);
    buf[n++] = 'm';
    sys_write(1, buf, n);
}

/* Read one keyboard event, blocking until one arrives. Returns the
 * event code (printable ASCII or KEY_*). */
static int read_key_blocking(void)
{
    return sys_tty_event_read(tty_fd, 1);
}

// ============================================================================
// Constants
// ============================================================================

#define SCREEN_WIDTH   40
#define SCREEN_HEIGHT  25
#define TEXT_ROWS      23   // Rows 1..23 for file content (row 0 = header, row 24 = status)
#define HEADER_ROW     0
#define STATUS_ROW     24

#define GAP_INITIAL    4096 // Initial gap size in words (characters)

// Palette indices
#define PAL_DEFAULT    0   // White on black  (normal text)
#define PAL_HEADER     1   // Black on white  (header bar - inverted)
#define PAL_STATUS     28  // Black on yellow (status bar - distinctive)
#define PAL_CURSOR     1   // Black on white  (cursor - inverted)
#define PAL_LINENUM    14  // Dark gray on black (tilde markers, dim)

// EOF sentinel (never appears in actual text)
#define CHAR_EOF       0

// Control keys (Ctrl+letter = 1..26)
#define CTRL_A  1
#define CTRL_C  3
#define CTRL_G  7
#define CTRL_H  8    // Backspace on some terminals
#define CTRL_L  12
#define CTRL_S  19
#define KEY_ESCAPE 27

// ============================================================================
// Gap buffer
// ============================================================================

// Forward declarations
void edit_int_to_str(int val, char *out);

unsigned int *buf;         // Heap-allocated buffer
int buf_size;              // Total buffer size in words
int gap_start;             // Index of first gap element
int gap_end;               // Index of first element after gap

int gap_content_length(void)
{
  return buf_size - (gap_end - gap_start);
}

int gap_char_at(int i)
{
  if (i < gap_start)
  {
    return buf[i];
  }
  return buf[i + (gap_end - gap_start)];
}

void gap_move_to(int pos)
{
  int gap_size;
  int i;

  if (pos == gap_start)
  {
    return;
  }

  gap_size = gap_end - gap_start;

  if (pos < gap_start)
  {
    i = gap_start - 1;
    while (i >= pos)
    {
      buf[i + gap_size] = buf[i];
      i--;
    }
  }
  else
  {
    i = gap_end;
    while (i < pos + gap_size)
    {
      buf[i - gap_size] = buf[i];
      i++;
    }
  }

  gap_end = pos + gap_size;
  gap_start = pos;
}

int gap_grow(void)
{
  int new_size;
  unsigned int *new_buf;
  int old_gap_size;
  int new_gap_end;
  int i;

  new_size = buf_size + GAP_INITIAL;
  new_buf = sys_heap_alloc(new_size);
  if (new_buf == (unsigned int *)0)
  {
    return 0;
  }

  for (i = 0; i < gap_start; i++)
  {
    new_buf[i] = buf[i];
  }

  old_gap_size = gap_end - gap_start;
  new_gap_end = gap_start + old_gap_size + GAP_INITIAL;

  for (i = gap_end; i < buf_size; i++)
  {
    new_buf[i + GAP_INITIAL] = buf[i];
  }

  for (i = gap_start; i < new_gap_end; i++)
  {
    new_buf[i] = 0;
  }

  buf = new_buf;
  buf_size = new_size;
  gap_end = new_gap_end;

  return 1;
}

void gap_insert(int ch)
{
  if (gap_start == gap_end)
  {
    if (!gap_grow())
    {
      return;
    }
  }
  buf[gap_start] = ch;
  gap_start++;
}

void gap_delete_before(void)
{
  if (gap_start > 0)
  {
    gap_start--;
  }
}

void gap_delete_after(void)
{
  if (gap_end < buf_size)
  {
    gap_end++;
  }
}

// ============================================================================
// Editor state
// ============================================================================

int cursor_line;
int cursor_col;
int scroll_y;
int scroll_x;
int total_lines;
int modified;
int running;

char filepath[128];
char filename[20];

// ============================================================================
// Line navigation helpers
// ============================================================================

int count_lines(void)
{
  int n;
  int len;
  int i;

  n = 1;
  len = gap_content_length();
  for (i = 0; i < len; i++)
  {
    if (gap_char_at(i) == '\n')
    {
      n++;
    }
  }
  return n;
}

int line_start_offset(int line)
{
  int off;
  int cur_line;
  int len;

  off = 0;
  cur_line = 0;
  len = gap_content_length();

  while (cur_line < line && off < len)
  {
    if (gap_char_at(off) == '\n')
    {
      cur_line++;
    }
    off++;
  }
  return off;
}

int line_length(int line)
{
  int off;
  int len_doc;
  int count;

  off = line_start_offset(line);
  len_doc = gap_content_length();
  count = 0;

  while (off + count < len_doc)
  {
    if (gap_char_at(off + count) == '\n')
    {
      break;
    }
    count++;
  }
  return count;
}

int line_col_to_offset(int line, int col)
{
  return line_start_offset(line) + col;
}

void clamp_cursor_col(void)
{
  int len;

  len = line_length(cursor_line);
  if (cursor_col > len)
  {
    cursor_col = len;
  }
}

// ============================================================================
// Rendering
// ============================================================================

void put_cell(int x, int y, int ch, int palette)
{
  char b;

  if (x < 0 || y < 0) return;
  /* Glyphs that overlap C0 control codes would confuse libterm v2's
   * ANSI parser — substitute a printable placeholder. The editor
   * normally only renders ASCII text and the ' ' / '~' markers. */
  if (ch < 0x20 || ch == 0x7F) {
    b = '?';
  } else {
    b = (char)ch;
  }
  ansi_set_palette(palette & 0xFF);
  ansi_goto(x, y);
  sys_write(1, &b, 1);
}

void render_header(void)
{
  int i;
  int c;

  for (i = 0; i < SCREEN_WIDTH; i++)
  {
    put_cell(i, HEADER_ROW, ' ', PAL_HEADER);
  }

  i = 1;
  c = 0;
  while (filename[c] != 0 && i < 20)
  {
    put_cell(i, HEADER_ROW, filename[c], PAL_HEADER);
    i++;
    c++;
  }

  if (modified)
  {
    put_cell(22, HEADER_ROW, '[', PAL_HEADER);
    put_cell(23, HEADER_ROW, 'm', PAL_HEADER);
    put_cell(24, HEADER_ROW, 'o', PAL_HEADER);
    put_cell(25, HEADER_ROW, 'd', PAL_HEADER);
    put_cell(26, HEADER_ROW, ']', PAL_HEADER);
  }

  {
    char numbuf[8];
    int pos;
    int j;

    pos = 30;

    edit_int_to_str(cursor_line + 1, numbuf);
    j = 0;
    while (numbuf[j] != 0 && pos < 35)
    {
      put_cell(pos, HEADER_ROW, numbuf[j], PAL_HEADER);
      pos++;
      j++;
    }

    put_cell(pos, HEADER_ROW, ':', PAL_HEADER);
    pos++;

    edit_int_to_str(cursor_col + 1, numbuf);
    j = 0;
    while (numbuf[j] != 0 && pos < SCREEN_WIDTH)
    {
      put_cell(pos, HEADER_ROW, numbuf[j], PAL_HEADER);
      pos++;
      j++;
    }
  }
}

void render_status(void)
{
  char *msg = "^S Save  Esc Quit";
  int i;

  for (i = 0; i < SCREEN_WIDTH; i++)
  {
    put_cell(i, STATUS_ROW, ' ', PAL_STATUS);
  }

  i = 1;
  while (*msg != 0 && i < SCREEN_WIDTH)
  {
    put_cell(i, STATUS_ROW, *msg, PAL_STATUS);
    msg++;
    i++;
  }
}

void render_text(void)
{
  int screen_row;
  int doc_line;
  int line_off;
  int line_len;
  int col;
  int doc_col;
  int ch;
  int len_doc;
  int pos;

  len_doc = gap_content_length();

  line_off = line_start_offset(scroll_y);

  for (screen_row = 0; screen_row < TEXT_ROWS; screen_row++)
  {
    doc_line = scroll_y + screen_row;

    if (doc_line < total_lines)
    {
      line_len = 0;
      pos = line_off;
      while (pos < len_doc && gap_char_at(pos) != '\n')
      {
        line_len++;
        pos++;
      }

      for (col = 0; col < SCREEN_WIDTH; col++)
      {
        doc_col = scroll_x + col;
        if (doc_col < line_len)
        {
          ch = gap_char_at(line_off + doc_col);
          put_cell(col, screen_row + 1, ch, PAL_DEFAULT);
        }
        else
        {
          put_cell(col, screen_row + 1, ' ', PAL_DEFAULT);
        }
      }

      if (pos < len_doc)
      {
        line_off = pos + 1;
      }
      else
      {
        line_off = pos;
      }
    }
    else
    {
      put_cell(0, screen_row + 1, '~', PAL_LINENUM);
      for (col = 1; col < SCREEN_WIDTH; col++)
      {
        put_cell(col, screen_row + 1, ' ', PAL_DEFAULT);
      }
    }
  }
}

void render_cursor(void)
{
  int screen_row;
  int screen_col;
  int ch;
  int off;
  int line_len;

  screen_row = cursor_line - scroll_y;
  screen_col = cursor_col - scroll_x;

  if (screen_row < 0 || screen_row >= TEXT_ROWS)
  {
    return;
  }
  if (screen_col < 0 || screen_col >= SCREEN_WIDTH)
  {
    return;
  }

  line_len = line_length(cursor_line);

  if (cursor_col < line_len)
  {
    off = line_col_to_offset(cursor_line, cursor_col);
    ch = gap_char_at(off);
  }
  else
  {
    ch = ' ';
  }

  put_cell(screen_col, screen_row + 1, ch, PAL_CURSOR);
}

void render_all(void)
{
  render_header();
  render_text();
  render_cursor();
  render_status();
}

// ============================================================================
// Scrolling
// ============================================================================

void ensure_cursor_visible(void)
{
  if (cursor_line < scroll_y)
  {
    scroll_y = cursor_line;
  }
  if (cursor_line >= scroll_y + TEXT_ROWS)
  {
    scroll_y = cursor_line - TEXT_ROWS + 1;
  }

  if (cursor_col < scroll_x)
  {
    scroll_x = cursor_col;
  }
  if (cursor_col >= scroll_x + SCREEN_WIDTH)
  {
    scroll_x = cursor_col - SCREEN_WIDTH + 1;
  }
}

// ============================================================================
// Cursor movement
// ============================================================================

void move_left(void)
{
  if (cursor_col > 0)
  {
    cursor_col--;
  }
  else if (cursor_line > 0)
  {
    cursor_line--;
    cursor_col = line_length(cursor_line);
  }
  ensure_cursor_visible();
}

void move_right(void)
{
  int len;

  len = line_length(cursor_line);
  if (cursor_col < len)
  {
    cursor_col++;
  }
  else if (cursor_line < total_lines - 1)
  {
    cursor_line++;
    cursor_col = 0;
  }
  ensure_cursor_visible();
}

void move_up(void)
{
  if (cursor_line > 0)
  {
    cursor_line--;
    clamp_cursor_col();
  }
  ensure_cursor_visible();
}

void move_down(void)
{
  if (cursor_line < total_lines - 1)
  {
    cursor_line++;
    clamp_cursor_col();
  }
  ensure_cursor_visible();
}

void move_home(void)
{
  cursor_col = 0;
  ensure_cursor_visible();
}

void move_end(void)
{
  cursor_col = line_length(cursor_line);
  ensure_cursor_visible();
}

void page_up(void)
{
  int i;

  for (i = 0; i < TEXT_ROWS; i++)
  {
    if (cursor_line > 0)
    {
      cursor_line--;
    }
  }
  clamp_cursor_col();
  ensure_cursor_visible();
}

void page_down(void)
{
  int i;

  for (i = 0; i < TEXT_ROWS; i++)
  {
    if (cursor_line < total_lines - 1)
    {
      cursor_line++;
    }
  }
  clamp_cursor_col();
  ensure_cursor_visible();
}

// ============================================================================
// Editing operations
// ============================================================================

void sync_gap_to_cursor(void)
{
  int off;

  off = line_col_to_offset(cursor_line, cursor_col);
  gap_move_to(off);
}

void do_insert_char(int ch)
{
  sync_gap_to_cursor();
  gap_insert(ch);
  cursor_col++;
  modified = 1;
  total_lines = count_lines();
  ensure_cursor_visible();
}

void do_insert_newline(void)
{
  sync_gap_to_cursor();
  gap_insert('\n');
  cursor_line++;
  cursor_col = 0;
  modified = 1;
  total_lines = count_lines();
  ensure_cursor_visible();
}

void do_backspace(void)
{
  if (cursor_col > 0)
  {
    sync_gap_to_cursor();
    gap_delete_before();
    cursor_col--;
    modified = 1;
  }
  else if (cursor_line > 0)
  {
    int prev_len;

    prev_len = line_length(cursor_line - 1);
    sync_gap_to_cursor();
    gap_delete_before();
    cursor_line--;
    cursor_col = prev_len;
    modified = 1;
  }
  total_lines = count_lines();
  ensure_cursor_visible();
}

void do_delete(void)
{
  int off;
  int len_doc;

  off = line_col_to_offset(cursor_line, cursor_col);
  len_doc = gap_content_length();

  if (off < len_doc)
  {
    sync_gap_to_cursor();
    gap_delete_after();
    modified = 1;
    total_lines = count_lines();
  }
}

void do_insert_tab(void)
{
  do_insert_char(' ');
  do_insert_char(' ');
}

// ============================================================================
// File I/O
// ============================================================================

int file_load(void)
{
  int fd;
  int fsize;
  int alloc_size;
  int bytes_remaining;
  int chunk;
  int bytes_read;
  unsigned char read_buf[256];
  int char_count;
  int dest_idx;
  int ri;
  int c;

  fd = sys_fs_open(filepath);
  if (fd < 0)
  {
    sys_putstr("edit: cannot open file\n");
    return -1;
  }

  fsize = sys_fs_filesize(fd);
  if (fsize < 0)
  {
    fsize = 0;
  }

  /* BRFS v2: filesize is in bytes. */
  alloc_size = fsize + GAP_INITIAL + 256;
  buf = sys_heap_alloc(alloc_size);
  if (buf == (unsigned int *)0)
  {
    sys_putstr("edit: out of memory\n");
    sys_fs_close(fd);
    return -1;
  }
  buf_size = alloc_size;

  gap_start = 0;
  gap_end = GAP_INITIAL;

  dest_idx = gap_end;
  bytes_remaining = fsize;
  char_count = 0;

  while (bytes_remaining > 0)
  {
    chunk = bytes_remaining;
    if (chunk > (int)sizeof(read_buf))
    {
      chunk = (int)sizeof(read_buf);
    }

    bytes_read = sys_fs_read(fd, read_buf, chunk);
    if (bytes_read <= 0)
    {
      break;
    }

    for (ri = 0; ri < bytes_read; ri++)
    {
      c = read_buf[ri];
      buf[dest_idx] = (unsigned int)c;
      dest_idx++;
      char_count++;
    }

    bytes_remaining = bytes_remaining - bytes_read;
  }

  sys_fs_close(fd);

  buf_size = dest_idx;

  return 0;
}

int file_save(void)
{
  int fd;
  int i;
  unsigned char chunk_buf[256];
  int chunk_idx;

  sys_fs_delete(filepath);
  sys_fs_create(filepath);

  fd = sys_fs_open(filepath);
  if (fd < 0)
  {
    return -1;
  }

  chunk_idx = 0;

  for (i = 0; i < gap_start; i++)
  {
    chunk_buf[chunk_idx++] = (unsigned char)(buf[i] & 0xFF);
    if (chunk_idx == (int)sizeof(chunk_buf))
    {
      sys_fs_write(fd, chunk_buf, chunk_idx);
      chunk_idx = 0;
    }
  }

  for (i = gap_end; i < buf_size; i++)
  {
    if (buf[i] == 0)
    {
      break;
    }
    chunk_buf[chunk_idx++] = (unsigned char)(buf[i] & 0xFF);
    if (chunk_idx == (int)sizeof(chunk_buf))
    {
      sys_fs_write(fd, chunk_buf, chunk_idx);
      chunk_idx = 0;
    }
  }

  if (chunk_idx > 0)
  {
    sys_fs_write(fd, chunk_buf, chunk_idx);
  }

  sys_fs_close(fd);
  modified = 0;
  return 0;
}

// ============================================================================
// User interaction
// ============================================================================

int wait_key(void)
{
  int key;

  while (1)
  {
    key = read_key_blocking();
    if (key != -1)
    {
      return key;
    }
  }
}

int confirm(char *prompt)
{
  int i;
  int key;

  for (i = 0; i < SCREEN_WIDTH; i++)
  {
    put_cell(i, STATUS_ROW, ' ', PAL_STATUS);
  }
  i = 1;
  while (*prompt != 0 && i < SCREEN_WIDTH)
  {
    put_cell(i, STATUS_ROW, *prompt, PAL_STATUS);
    prompt++;
    i++;
  }

  key = wait_key();
  render_status();

  if (key == 'y' || key == 'Y')
  {
    return 1;
  }
  return 0;
}

// ============================================================================
// Helper: integer to string
// ============================================================================

void edit_int_to_str(int val, char *out)
{
  char tmp[12];
  int i;
  int j;
  int neg;

  neg = 0;
  if (val < 0)
  {
    neg = 1;
    val = -val;
  }

  i = 0;
  if (val == 0)
  {
    tmp[i] = '0';
    i++;
  }
  else
  {
    while (val > 0)
    {
      tmp[i] = '0' + (val % 10);
      val = val / 10;
      i++;
    }
  }

  j = 0;
  if (neg)
  {
    out[j] = '-';
    j++;
  }
  while (i > 0)
  {
    i--;
    out[j] = tmp[i];
    j++;
  }

  out[j] = 0;
}

// ============================================================================
// String helpers
// ============================================================================

int str_len(char *s)
{
  int n;

  n = 0;
  while (s[n] != 0)
  {
    n++;
  }
  return n;
}

void str_copy(char *dest, char *src)
{
  while (*src != 0)
  {
    *dest = *src;
    dest++;
    src++;
  }
  *dest = 0;
}

void str_cat(char *dest, char *src)
{
  while (*dest != 0)
  {
    dest++;
  }
  str_copy(dest, src);
}

void extract_basename(char *path, char *out)
{
  int last_slash;
  int i;

  last_slash = -1;
  i = 0;
  while (path[i] != 0)
  {
    if (path[i] == '/')
    {
      last_slash = i;
    }
    i++;
  }

  if (last_slash >= 0)
  {
    str_copy(out, &path[last_slash + 1]);
  }
  else
  {
    str_copy(out, path);
  }
}

// ============================================================================
// Main
// ============================================================================

int main(void)
{
  int argc;
  char **argv;
  char *cwd;
  int key;

  argc = sys_shell_argc();
  argv = sys_shell_argv();

  if (argc < 2)
  {
    sys_putstr("Usage: edit <file>\n");
    return 1;
  }

  filepath[0] = 0;
  if (argv[1][0] != '/')
  {
    cwd = sys_shell_getcwd();
    str_copy(filepath, cwd);
    str_cat(filepath, "/");
    str_cat(filepath, argv[1]);
  }
  else
  {
    str_copy(filepath, argv[1]);
  }

  extract_basename(filepath, filename);

  if (file_load() != 0)
  {
    return 1;
  }

  cursor_line = 0;
  cursor_col = 0;
  scroll_y = 0;
  scroll_x = 0;
  modified = 0;
  running = 1;
  total_lines = count_lines();

  /* Enter alternate screen so the prior shell view is restored on exit. */
  ansi_write("\x1b[?1049h");
  /* Disable auto-wrap (DECAWM): prevents the bottom-right cell write
     from triggering a scroll that would shift the header off-screen. */
  ansi_write("\x1b[?7l");

  tty_fd = sys_tty_open_raw(0 /* blocking */);
  if (tty_fd < 0)
  {
    ansi_write("\x1b[?1049l");
    sys_putstr("edit: cannot open /dev/tty\n");
    return 1;
  }

  /* Clear the alt screen and home the cursor. */
  ansi_write("\x1b[2J\x1b[H");
  last_palette = -1;
  render_all();

  while (running)
  {
    key = read_key_blocking();
    if (key < 0)
    {
      continue;
    }

    if (key == KEY_LEFT)
    {
      move_left();
    }
    else if (key == KEY_RIGHT)
    {
      move_right();
    }
    else if (key == KEY_UP)
    {
      move_up();
    }
    else if (key == KEY_DOWN)
    {
      move_down();
    }
    else if (key == KEY_HOME)
    {
      move_home();
    }
    else if (key == KEY_END)
    {
      move_end();
    }
    else if (key == KEY_PAGEUP)
    {
      page_up();
    }
    else if (key == KEY_PAGEDOWN)
    {
      page_down();
    }
    else if (key == KEY_DELETE)
    {
      do_delete();
    }
    else if (key == 0x08 || key == 127)
    {
      do_backspace();
    }
    else if (key == '\n')
    {
      do_insert_newline();
    }
    else if (key == '\t')
    {
      do_insert_tab();
    }
    else if (key == CTRL_S)
    {
      if (file_save() == 0)
      {
        int si;
        for (si = 0; si < SCREEN_WIDTH; si++)
        {
          put_cell(si, STATUS_ROW, ' ', PAL_STATUS);
        }
        put_cell(1, STATUS_ROW, 'S', PAL_STATUS);
        put_cell(2, STATUS_ROW, 'a', PAL_STATUS);
        put_cell(3, STATUS_ROW, 'v', PAL_STATUS);
        put_cell(4, STATUS_ROW, 'e', PAL_STATUS);
        put_cell(5, STATUS_ROW, 'd', PAL_STATUS);
        put_cell(6, STATUS_ROW, '!', PAL_STATUS);
      }
      else
      {
        int si;
        for (si = 0; si < SCREEN_WIDTH; si++)
        {
          put_cell(si, STATUS_ROW, ' ', PAL_STATUS);
        }
        put_cell(1, STATUS_ROW, 'S', PAL_STATUS);
        put_cell(2, STATUS_ROW, 'a', PAL_STATUS);
        put_cell(3, STATUS_ROW, 'v', PAL_STATUS);
        put_cell(4, STATUS_ROW, 'e', PAL_STATUS);
        put_cell(5, STATUS_ROW, ' ', PAL_STATUS);
        put_cell(6, STATUS_ROW, 'E', PAL_STATUS);
        put_cell(7, STATUS_ROW, 'R', PAL_STATUS);
        put_cell(8, STATUS_ROW, 'R', PAL_STATUS);
      }
      continue;
    }
    else if (key == KEY_ESCAPE)
    {
      if (modified)
      {
        if (confirm("Unsaved changes! Quit? (y/n)"))
        {
          running = 0;
        }
      }
      else
      {
        running = 0;
      }
      if (!running)
      {
        continue;
      }
    }
    else if (key == CTRL_L)
    {
      // Refresh screen
    }
    else if (key >= 32 && key < 127)
    {
      do_insert_char(key);
    }
    else
    {
      continue;
    }

    render_all();
  }

  /* Re-enable auto-wrap and leave the alternate screen — restores
   * whatever was on screen before. */
  ansi_write("\x1b[?7h");
  ansi_write("\x1b[?1049l");
  if (tty_fd >= 0) sys_close(tty_fd);

  return 0;
}
