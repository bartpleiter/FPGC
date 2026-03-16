// edit.c — Text editor for FPGC BDOS
// A nano-like terminal text editor using a gap buffer and syscalls.
// Usage: edit <filename>

#define USER_SYSCALL
#include "libs/user/user.h"

// ============================================================================
// Constants
// ============================================================================

#define SCREEN_WIDTH   40
#define SCREEN_HEIGHT  25
#define TEXT_ROWS      23   // Rows 1..23 for file content (row 0 = header, row 24 = status)
#define HEADER_ROW     0
#define STATUS_ROW     24

#define GAP_INITIAL    4096 // Initial gap size in words (characters)

// Palette indices (packed into tile_palette arg as lower 8 bits)
// These must match the kernel palette table loaded at boot (see gpu_data_ascii.h)
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

// The gap buffer stores text as:
//   [pre-gap text] [GAP] [post-gap text]
// buf[0..gap_start-1]        = text before cursor
// buf[gap_end..buf_size-1]   = text after cursor
// The cursor is at the left edge of the gap.

unsigned int* buf;         // Heap-allocated buffer
int buf_size;              // Total buffer size in words
int gap_start;             // Index of first gap element
int gap_end;               // Index of first element after gap

// Characters in the buffer (excluding the gap)
int gap_content_length()
{
  return buf_size - (gap_end - gap_start);
}

// Character at logical position i (0-indexed, gap-transparent)
int gap_char_at(int i)
{
  if (i < gap_start)
  {
    return buf[i];
  }
  return buf[i + (gap_end - gap_start)];
}

// Move gap so gap_start == pos
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
    // Move characters from before gap into the end of gap
    i = gap_start - 1;
    while (i >= pos)
    {
      buf[i + gap_size] = buf[i];
      i--;
    }
  }
  else
  {
    // Move characters from after gap into the beginning of gap
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

// Grow the buffer by allocating a new one and copying contents.
// The old buffer is "leaked" (bump allocator frees all on program exit).
// Returns 1 on success, 0 on failure.
int gap_grow()
{
  int new_size;
  unsigned int* new_buf;
  int old_gap_size;
  int new_gap_end;
  int i;

  new_size = buf_size + GAP_INITIAL;
  new_buf = sys_heap_alloc(new_size);
  if (new_buf == (unsigned int*)0)
  {
    return 0;
  }

  // Copy pre-gap content
  for (i = 0; i < gap_start; i++)
  {
    new_buf[i] = buf[i];
  }

  // New gap is GAP_INITIAL words larger
  old_gap_size = gap_end - gap_start;
  new_gap_end = gap_start + old_gap_size + GAP_INITIAL;

  // Copy post-gap content
  for (i = gap_end; i < buf_size; i++)
  {
    new_buf[i + GAP_INITIAL] = buf[i];
  }

  // Zero the new gap area
  for (i = gap_start; i < new_gap_end; i++)
  {
    new_buf[i] = 0;
  }

  buf = new_buf;
  buf_size = new_size;
  gap_end = new_gap_end;

  return 1;
}

// Insert a character at the current gap position
void gap_insert(int ch)
{
  if (gap_start == gap_end)
  {
    // Gap exhausted — grow the buffer
    if (!gap_grow())
    {
      return; // Out of memory, silently fail
    }
  }
  buf[gap_start] = ch;
  gap_start++;
}

// Delete the character before the gap (backspace)
void gap_delete_before()
{
  if (gap_start > 0)
  {
    gap_start--;
  }
}

// Delete the character after the gap (delete key)
void gap_delete_after()
{
  if (gap_end < buf_size)
  {
    gap_end++;
  }
}

// ============================================================================
// Editor state
// ============================================================================

int cursor_line;       // Cursor line in the document (0-based)
int cursor_col;        // Cursor column in the document (0-based)
int scroll_y;          // First visible line (0-based)
int scroll_x;          // First visible column (0-based, for horizontal scrolling)
int total_lines;       // Total number of lines (always >= 1)
int modified;          // Dirty flag
int running;           // Main loop flag

char filepath[128];    // Absolute path of the file being edited
char filename[20];     // Display name (basename)

// ============================================================================
// Line navigation helpers (operate on the gap buffer)
// ============================================================================

// Count total lines in the buffer (number of '\n' + 1)
int count_lines()
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

// Return the logical offset of the start of line `line` (0-based)
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

// Return the length of line `line` (not including the '\n')
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

// Convert (line, col) to a logical offset in the gap buffer
int line_col_to_offset(int line, int col)
{
  return line_start_offset(line) + col;
}

// Ensure cursor_col is within the current line length
void clamp_cursor_col()
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

// Helper: write a character+palette to a screen cell
void put_cell(int x, int y, int ch, int palette)
{
  sys_term_put_cell(x, y, (ch << 8) | (palette & 0xFF));
}

// Render the header line (row 0)
void render_header()
{
  int i;
  int c;

  // Fill header with spaces
  for (i = 0; i < SCREEN_WIDTH; i++)
  {
    put_cell(i, HEADER_ROW, ' ', PAL_HEADER);
  }

  // Write filename
  i = 1;
  c = 0;
  while (filename[c] != 0 && i < 20)
  {
    put_cell(i, HEADER_ROW, filename[c], PAL_HEADER);
    i++;
    c++;
  }

  // Write [mod] if modified
  if (modified)
  {
    put_cell(22, HEADER_ROW, '[', PAL_HEADER);
    put_cell(23, HEADER_ROW, 'm', PAL_HEADER);
    put_cell(24, HEADER_ROW, 'o', PAL_HEADER);
    put_cell(25, HEADER_ROW, 'd', PAL_HEADER);
    put_cell(26, HEADER_ROW, ']', PAL_HEADER);
  }

  // Write line:col at the right side
  {
    char numbuf[8];
    int pos;
    int j;

    pos = 30;

    // Line number
    sys_int_to_str(cursor_line + 1, numbuf);
    j = 0;
    while (numbuf[j] != 0 && pos < 35)
    {
      put_cell(pos, HEADER_ROW, numbuf[j], PAL_HEADER);
      pos++;
      j++;
    }

    put_cell(pos, HEADER_ROW, ':', PAL_HEADER);
    pos++;

    // Column number
    sys_int_to_str(cursor_col + 1, numbuf);
    j = 0;
    while (numbuf[j] != 0 && pos < SCREEN_WIDTH)
    {
      put_cell(pos, HEADER_ROW, numbuf[j], PAL_HEADER);
      pos++;
      j++;
    }
  }
}

// Render the status bar (row 24)
void render_status()
{
  char* msg = "^S Save  Esc Quit";
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

// Render visible text lines (rows 1..TEXT_ROWS)
// Compute first visible line offset once, then scan forward.
void render_text()
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

  // Find the offset of the first visible line (scroll_y) — single scan from start
  line_off = line_start_offset(scroll_y);

  for (screen_row = 0; screen_row < TEXT_ROWS; screen_row++)
  {
    doc_line = scroll_y + screen_row;

    if (doc_line < total_lines)
    {
      // Compute line length by scanning from line_off (cheap — single line scan)
      line_len = 0;
      pos = line_off;
      while (pos < len_doc && gap_char_at(pos) != '\n')
      {
        line_len++;
        pos++;
      }

      // Render this line
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

      // Advance line_off to the start of the next line
      // pos is at the '\n' (or at len_doc if last line has no trailing newline)
      if (pos < len_doc)
      {
        line_off = pos + 1; // skip past the '\n'
      }
      else
      {
        line_off = pos;
      }
    }
    else
    {
      // Below end of file: show '~' marker in first column
      put_cell(0, screen_row + 1, '~', PAL_LINENUM);
      for (col = 1; col < SCREEN_WIDTH; col++)
      {
        put_cell(col, screen_row + 1, ' ', PAL_DEFAULT);
      }
    }
  }
}

// Render cursor (highlight the cell under cursor)
void render_cursor()
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

// Full screen redraw
void render_all()
{
  render_header();
  render_text();
  render_cursor();
  render_status();
}

// ============================================================================
// Scrolling
// ============================================================================

void ensure_cursor_visible()
{
  // Vertical scrolling
  if (cursor_line < scroll_y)
  {
    scroll_y = cursor_line;
  }
  if (cursor_line >= scroll_y + TEXT_ROWS)
  {
    scroll_y = cursor_line - TEXT_ROWS + 1;
  }

  // Horizontal scrolling
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

void move_left()
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

void move_right()
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

void move_up()
{
  if (cursor_line > 0)
  {
    cursor_line--;
    clamp_cursor_col();
  }
  ensure_cursor_visible();
}

void move_down()
{
  if (cursor_line < total_lines - 1)
  {
    cursor_line++;
    clamp_cursor_col();
  }
  ensure_cursor_visible();
}

void move_home()
{
  cursor_col = 0;
  ensure_cursor_visible();
}

void move_end()
{
  cursor_col = line_length(cursor_line);
  ensure_cursor_visible();
}

void page_up()
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

void page_down()
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

void sync_gap_to_cursor()
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

void do_insert_newline()
{
  sync_gap_to_cursor();
  gap_insert('\n');
  cursor_line++;
  cursor_col = 0;
  modified = 1;
  total_lines = count_lines();
  ensure_cursor_visible();
}

void do_backspace()
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
    // Join with previous line
    int prev_len;

    prev_len = line_length(cursor_line - 1);
    sync_gap_to_cursor();
    gap_delete_before(); // Deletes the '\n' that ended the previous line
    cursor_line--;
    cursor_col = prev_len;
    modified = 1;
  }
  total_lines = count_lines();
  ensure_cursor_visible();
}

void do_delete()
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

void do_insert_tab()
{
  do_insert_char(' ');
  do_insert_char(' ');
}

// ============================================================================
// File I/O
// ============================================================================

int file_load()
{
  int fd;
  int fsize;
  int alloc_size;
  int words_remaining;
  int chunk;
  int words_read;
  unsigned int read_buf[256];
  int char_count;
  int dest_idx;
  int ri;
  int b;
  int c;
  int done;

  fd = sys_fs_open(filepath);
  if (fd < 0)
  {
    sys_print_str("edit: cannot open file\n");
    return -1;
  }

  fsize = sys_fs_filesize(fd);
  if (fsize < 0)
  {
    fsize = 0;
  }

  // Each BRFS word holds 4 packed bytes
  alloc_size = fsize * 4 + GAP_INITIAL + 256;
  buf = sys_heap_alloc(alloc_size);
  if (buf == (unsigned int*)0)
  {
    sys_print_str("edit: out of memory\n");
    sys_fs_close(fd);
    return -1;
  }
  buf_size = alloc_size;

  // Read file into buffer after the gap, unpacking 4 chars per word
  gap_start = 0;
  gap_end = GAP_INITIAL;

  dest_idx = gap_end;
  words_remaining = fsize;
  char_count = 0;
  done = 0;

  while (words_remaining > 0 && !done)
  {
    chunk = words_remaining;
    if (chunk > 256)
    {
      chunk = 256;
    }

    words_read = sys_fs_read(fd, read_buf, chunk);
    if (words_read <= 0)
    {
      break;
    }

    // Unpack each word into up to 4 characters (big-endian)
    for (ri = 0; ri < words_read && !done; ri++)
    {
      for (b = 3; b >= 0; b--)
      {
        c = (read_buf[ri] >> (b * 8)) & 0xFF;
        if (c == 0)
        {
          done = 1;
          break;
        }
        buf[dest_idx] = (unsigned int)c;
        dest_idx++;
        char_count++;
      }
    }

    words_remaining = words_remaining - words_read;
  }

  sys_fs_close(fd);

  // Set buf_size to actual content loaded + gap
  buf_size = dest_idx;

  return 0;
}

int file_save()
{
  int fd;
  int i;
  unsigned int chunk_buf[64];
  int chunk_idx;
  unsigned int current_word;
  int byte_pos;

  // Delete and recreate the file to reset size
  sys_fs_delete(filepath);
  sys_fs_create(filepath);

  fd = sys_fs_open(filepath);
  if (fd < 0)
  {
    return -1;
  }

  // Pack characters 4-per-word (big-endian) and write in chunks
  chunk_idx = 0;
  current_word = 0;
  byte_pos = 0;

  // Write pre-gap content
  for (i = 0; i < gap_start; i++)
  {
    current_word = (current_word << 8) | (buf[i] & 0xFF);
    byte_pos++;
    if (byte_pos == 4)
    {
      chunk_buf[chunk_idx] = current_word;
      chunk_idx++;
      current_word = 0;
      byte_pos = 0;
      if (chunk_idx == 64)
      {
        sys_fs_write(fd, chunk_buf, 64);
        chunk_idx = 0;
      }
    }
  }

  // Write post-gap content
  for (i = gap_end; i < buf_size; i++)
  {
    // Stop at first zero (end of content)
    if (buf[i] == 0)
    {
      break;
    }
    current_word = (current_word << 8) | (buf[i] & 0xFF);
    byte_pos++;
    if (byte_pos == 4)
    {
      chunk_buf[chunk_idx] = current_word;
      chunk_idx++;
      current_word = 0;
      byte_pos = 0;
      if (chunk_idx == 64)
      {
        sys_fs_write(fd, chunk_buf, 64);
        chunk_idx = 0;
      }
    }
  }

  // Flush remaining partial word (pad with zeros)
  if (byte_pos > 0)
  {
    current_word = current_word << (8 * (4 - byte_pos));
    chunk_buf[chunk_idx] = current_word;
    chunk_idx++;
  }

  // Flush remaining chunk
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

// Wait for a keypress (blocking)
int wait_key()
{
  int key;

  while (1)
  {
    key = sys_read_key();
    if (key != -1)
    {
      return key;
    }
  }
}

// Ask yes/no confirmation. Returns 1 for yes, 0 for no.
int confirm(char* prompt)
{
  int i;
  int key;

  // Show prompt on status bar
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
  render_status(); // Restore normal status bar

  if (key == 'y' || key == 'Y')
  {
    return 1;
  }
  return 0;
}

// ============================================================================
// Helper: integer to string (simple, no stdlib dependency)
// ============================================================================

void sys_int_to_str(int val, char* out)
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
  // Reverse
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

int str_len(char* s)
{
  int n;

  n = 0;
  while (s[n] != 0)
  {
    n++;
  }
  return n;
}

void str_copy(char* dest, char* src)
{
  while (*src != 0)
  {
    *dest = *src;
    dest++;
    src++;
  }
  *dest = 0;
}

void str_cat(char* dest, char* src)
{
  while (*dest != 0)
  {
    dest++;
  }
  str_copy(dest, src);
}

// Extract basename from a path
void extract_basename(char* path, char* out)
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
// Setup palettes via terminal put_cell (using existing palette table)
// ============================================================================

// Palettes are set by the kernel's pattern/palette tables.
// We use palette indices that match the ones BDOS has already loaded.
// PAL_DEFAULT = 0 (white on black, default terminal palette)
// PAL_HEADER  = 1 (we'll render with inverted look using palette slot 1)
// PAL_STATUS  = 2
// PAL_CURSOR  = 3
// PAL_LINENUM = 4
//
// Since we can't modify palette memory from user space via the current
// syscall interface, we reuse the default palette (0) for text and use
// the inverted palette the shell uses for cursor rendering.
// For a first version, we just differentiate by character choices
// and leave palette 0 everywhere except the cursor.
//
// Actually, the BDOS shell already uses palette inversion for its cursor.
// Let's check what palettes are available. The term library initializes
// some palettes. For now, we'll use PAL_DEFAULT (0) for everything
// except the cursor, where we'll use a non-zero palette that the
// BDOS terminal has set up for contrast. If palette 1 is used for
// something visible, the cursor will stand out.

// ============================================================================
// Main
// ============================================================================

int main()
{
  int argc;
  char** argv;
  char* cwd;
  int key;

  argc = sys_shell_argc();
  argv = sys_shell_argv();

  if (argc < 2)
  {
    sys_print_str("Usage: edit <file>\n");
    return 1;
  }

  // Build absolute path
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

  // Extract basename for display
  extract_basename(filepath, filename);

  // Load file
  if (file_load() != 0)
  {
    return 1;
  }

  // Initialize editor state
  cursor_line = 0;
  cursor_col = 0;
  scroll_y = 0;
  scroll_x = 0;
  modified = 0;
  running = 1;
  total_lines = count_lines();

  // Clear screen and draw
  sys_term_clear();
  render_all();

  // ---- Main loop ----
  while (running)
  {
    key = sys_read_key();
    if (key == -1)
    {
      continue;
    }

    // Dispatch key
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
    else if (key == 0x08 || key == 127) // Backspace
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
        // Brief "Saved" feedback on status bar
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
      // Don't redraw status yet — let user see the message until next keypress
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
      // Printable ASCII
      do_insert_char(key);
    }
    else
    {
      // Ignore unknown keys
      continue;
    }

    // Redraw
    render_all();
  }

  // Restore terminal
  sys_term_clear();
  sys_term_set_cursor(0, 0);

  return 0;
}
