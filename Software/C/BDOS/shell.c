/*
 * BDOS shell module.
 * Provides a simple interactive command shell.
 */

#include "BDOS/bdos.h"

char bdos_shell_input[BDOS_SHELL_INPUT_MAX];
int bdos_shell_input_len;
int bdos_shell_input_overflow;
char bdos_shell_prompt[BDOS_SHELL_PROMPT_MAX];
char bdos_shell_cwd[BDOS_SHELL_PATH_MAX] = "/";
unsigned int bdos_shell_prompt_x;
unsigned int bdos_shell_prompt_y;
int bdos_shell_last_render_len;
int bdos_shell_cursor_index;
int bdos_shell_cursor_visible;
unsigned int bdos_shell_cursor_draw_x;
unsigned int bdos_shell_cursor_draw_y;
unsigned char bdos_shell_cursor_saved_tile;
unsigned char bdos_shell_cursor_saved_palette;
unsigned int bdos_shell_start_micros;

#define BDOS_SHELL_HISTORY_SIZE 8
char bdos_shell_history[BDOS_SHELL_HISTORY_SIZE][BDOS_SHELL_INPUT_MAX];
int bdos_shell_history_head;
int bdos_shell_history_count;
int bdos_shell_history_nav_offset;
char bdos_shell_history_saved_input[BDOS_SHELL_INPUT_MAX];

/* ------------------------------------------------------------------------- */
/* Prompt and cursor rendering                                                */
/* ------------------------------------------------------------------------- */

// Build the dynamic prompt string, currently carrying a future-ready cwd field.
void bdos_shell_build_prompt()
{
  bdos_shell_prompt[0] = '\0';
  strcat(bdos_shell_prompt, bdos_shell_cwd);
  strcat(bdos_shell_prompt, "> ");
}

int bdos_shell_prompt_len()
{
  return strlen(bdos_shell_prompt);
}

// Draw visual cursor by inverting the palette of the character under the cursor.
void bdos_shell_draw_cursor()
{
  unsigned int x;
  unsigned int y;
  unsigned char tile;
  unsigned char palette;

  term_get_cursor(&x, &y);
  term_get_cell(x, y, &tile, &palette);

  bdos_shell_cursor_draw_x = x;
  bdos_shell_cursor_draw_y = y;
  bdos_shell_cursor_saved_tile = tile;
  bdos_shell_cursor_saved_palette = palette;
  bdos_shell_cursor_visible = 1;

  term_put_cell(x, y, tile, PALETTE_BLACK_ON_WHITE);
}

// Restore character/palette beneath previously drawn cursor overlay.
void bdos_shell_clear_cursor()
{
  if (!bdos_shell_cursor_visible)
  {
    return;
  }

  term_put_cell(
    bdos_shell_cursor_draw_x,
    bdos_shell_cursor_draw_y,
    bdos_shell_cursor_saved_tile,
    bdos_shell_cursor_saved_palette);
  bdos_shell_cursor_visible = 0;
}

// Map editor cursor index to terminal coordinates and update terminal cursor.
void bdos_shell_move_cursor_to_input_index()
{
  int prompt_len;
  int absolute_char_index;
  int cursor_row;
  int cursor_col;

  prompt_len = bdos_shell_prompt_len();

  if (bdos_shell_cursor_index < 0)
  {
    bdos_shell_cursor_index = 0;
  }
  if (bdos_shell_cursor_index > bdos_shell_input_len)
  {
    bdos_shell_cursor_index = bdos_shell_input_len;
  }

  absolute_char_index = prompt_len + bdos_shell_cursor_index;
  cursor_row = (int)bdos_shell_prompt_y + ((int)bdos_shell_prompt_x + absolute_char_index) / TERM_WIDTH;
  cursor_col = ((int)bdos_shell_prompt_x + absolute_char_index) % TERM_WIDTH;

  if (cursor_row < 0)
  {
    cursor_row = 0;
  }
  if (cursor_row >= TERM_HEIGHT)
  {
    cursor_row = TERM_HEIGHT - 1;
  }

  term_set_cursor((unsigned int)cursor_col, (unsigned int)cursor_row);
}

// Compensate stored prompt anchor when render operation causes terminal scroll.
void bdos_shell_adjust_prompt_after_render(int rendered_chars)
{
  int rows_used;
  int overflow_rows;
  int prompt_y;

  rows_used = (bdos_shell_prompt_x + rendered_chars) / TERM_WIDTH;
  overflow_rows = ((int)bdos_shell_prompt_y + rows_used) - (TERM_HEIGHT - 1);

  if (overflow_rows > 0)
  {
    prompt_y = (int)bdos_shell_prompt_y - overflow_rows;
    if (prompt_y < 0)
    {
      prompt_y = 0;
    }
    bdos_shell_prompt_y = (unsigned int)prompt_y;
  }
}

// Re-render complete editable line (prompt + input) and redraw visual cursor.
void bdos_shell_render_line()
{
  int i;
  int new_render_len;

  bdos_shell_clear_cursor();
  term_set_palette(PALETTE_WHITE_ON_BLACK);
  term_set_cursor(bdos_shell_prompt_x, bdos_shell_prompt_y);
  for (i = 0; i < bdos_shell_last_render_len; i++)
  {
    term_putchar(' ');
  }

  bdos_shell_adjust_prompt_after_render(bdos_shell_last_render_len);

  term_set_cursor(bdos_shell_prompt_x, bdos_shell_prompt_y);
  term_puts(bdos_shell_prompt);
  term_write(bdos_shell_input, bdos_shell_input_len);
  new_render_len = bdos_shell_prompt_len() + bdos_shell_input_len;
  bdos_shell_last_render_len = new_render_len;
  bdos_shell_adjust_prompt_after_render(new_render_len);

  bdos_shell_move_cursor_to_input_index();
  bdos_shell_draw_cursor();
}

/* ------------------------------------------------------------------------- */
/* History management                                                         */
/* ------------------------------------------------------------------------- */

int bdos_shell_history_index_from_offset(int nav_offset)
{
  int idx;

  idx = bdos_shell_history_head - 1 - nav_offset;
  while (idx < 0)
  {
    idx += BDOS_SHELL_HISTORY_SIZE;
  }
  return idx % BDOS_SHELL_HISTORY_SIZE;
}

// Replace current editable input buffer with provided text.
void bdos_shell_set_input(char* source)
{
  int len;

  len = strlen(source);
  if (len >= BDOS_SHELL_INPUT_MAX)
  {
    len = BDOS_SHELL_INPUT_MAX - 1;
  }

  memcpy(bdos_shell_input, source, len);
  bdos_shell_input[len] = '\0';
  bdos_shell_input_len = len;
  bdos_shell_cursor_index = bdos_shell_input_len;
  bdos_shell_input_overflow = 0;
}

// Push a non-empty command into fixed-size ring history.
void bdos_shell_history_add(char* line)
{
  int len;

  len = strlen(line);
  if (len == 0)
  {
    return;
  }

  if (len >= BDOS_SHELL_INPUT_MAX)
  {
    len = BDOS_SHELL_INPUT_MAX - 1;
  }

  memcpy(bdos_shell_history[bdos_shell_history_head], line, len);
  bdos_shell_history[bdos_shell_history_head][len] = '\0';

  bdos_shell_history_head = (bdos_shell_history_head + 1) % BDOS_SHELL_HISTORY_SIZE;
  if (bdos_shell_history_count < BDOS_SHELL_HISTORY_SIZE)
  {
    bdos_shell_history_count++;
  }
}

// Navigate to older history entry.
void bdos_shell_history_nav_up()
{
  int idx;

  if (bdos_shell_history_count == 0)
  {
    return;
  }

  if (bdos_shell_history_nav_offset < 0)
  {
    memcpy(bdos_shell_history_saved_input, bdos_shell_input, bdos_shell_input_len);
    bdos_shell_history_saved_input[bdos_shell_input_len] = '\0';
    bdos_shell_history_nav_offset = 0;
  }
  else if (bdos_shell_history_nav_offset < (bdos_shell_history_count - 1))
  {
    bdos_shell_history_nav_offset++;
  }

  idx = bdos_shell_history_index_from_offset(bdos_shell_history_nav_offset);
  bdos_shell_set_input(bdos_shell_history[idx]);
}

// Navigate to newer history entry or restore temporary draft input.
void bdos_shell_history_nav_down()
{
  int idx;

  if (bdos_shell_history_nav_offset < 0)
  {
    return;
  }

  if (bdos_shell_history_nav_offset == 0)
  {
    bdos_shell_history_nav_offset = -1;
    bdos_shell_set_input(bdos_shell_history_saved_input);
    return;
  }

  bdos_shell_history_nav_offset--;
  idx = bdos_shell_history_index_from_offset(bdos_shell_history_nav_offset);
  bdos_shell_set_input(bdos_shell_history[idx]);
}

/* ------------------------------------------------------------------------- */
/* Line editor                                                                */
/* ------------------------------------------------------------------------- */

// Insert character at current cursor index.
void bdos_shell_insert_char(char c)
{
  int i;

  if (bdos_shell_input_len >= (BDOS_SHELL_INPUT_MAX - 1))
  {
    bdos_shell_input_overflow = 1;
    return;
  }

  for (i = bdos_shell_input_len; i >= bdos_shell_cursor_index; i--)
  {
    bdos_shell_input[i + 1] = bdos_shell_input[i];
  }

  bdos_shell_input[bdos_shell_cursor_index] = c;
  bdos_shell_input_len++;
  bdos_shell_cursor_index++;
  bdos_shell_input_overflow = 0;
}

// Remove character immediately left of cursor.
void bdos_shell_backspace_char()
{
  int i;

  if (bdos_shell_cursor_index <= 0)
  {
    return;
  }

  for (i = bdos_shell_cursor_index - 1; i < bdos_shell_input_len; i++)
  {
    bdos_shell_input[i] = bdos_shell_input[i + 1];
  }

  bdos_shell_input_len--;
  bdos_shell_cursor_index--;
  bdos_shell_input_overflow = 0;
}

// Remove character immediately right of cursor.
void bdos_shell_delete_char()
{
  int i;

  if (bdos_shell_cursor_index >= bdos_shell_input_len)
  {
    return;
  }

  for (i = bdos_shell_cursor_index; i < bdos_shell_input_len; i++)
  {
    bdos_shell_input[i] = bdos_shell_input[i + 1];
  }

  bdos_shell_input_len--;
  bdos_shell_input_overflow = 0;
}

// Start a new shell input line by recording prompt anchor and rendering prompt.
void bdos_shell_start_line()
{
  bdos_shell_build_prompt();
  term_get_cursor(&bdos_shell_prompt_x, &bdos_shell_prompt_y);
  bdos_shell_last_render_len = 0;
  bdos_shell_render_line();
}

// Print compact startup banner.
void bdos_shell_print_welcome()
{
  term_puts(" ___ ___   ___  ___ \n");
  term_puts("| _ )   \\ / _ \\/ __|\n");
  term_puts("| _ \\ |) | (_) \\__ \\\n");
  term_puts("|___/___/ \\___/|___/v2.0-dev2\n\n");
}

/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/* Command submit lifecycle                                                   */
/* ------------------------------------------------------------------------- */

// Submit current input line and start next prompt line.
void bdos_shell_submit_input()
{
  bdos_shell_clear_cursor();
  term_putchar('\n');

  if (bdos_shell_input_overflow)
  {
    term_puts("error: input too long\n");
  }
  else
  {
    bdos_shell_input[bdos_shell_input_len] = '\0';

    if (bdos_shell_handle_special_mode_line(bdos_shell_input))
    {
      // Special-mode line consumed (boot format confirmation / format wizard).
    }
    else
    {
      bdos_shell_history_add(bdos_shell_input);
      bdos_shell_execute_line(bdos_shell_input);
    }
  }

  bdos_shell_input_len = 0;
  bdos_shell_cursor_index = 0;
  bdos_shell_input[0] = '\0';
  bdos_shell_input_overflow = 0;
  bdos_shell_history_nav_offset = -1;
  bdos_shell_history_saved_input[0] = '\0';
  bdos_shell_start_line();
}

/* ------------------------------------------------------------------------- */
/* Event handling and lifecycle                                                */
/* ------------------------------------------------------------------------- */

// Handle one shell key event from keyboard FIFO.
void bdos_shell_handle_key_event(int key_event)
{
  // Enter/Return: submit the current input line for parsing and execution.
  if (key_event == '\n' || key_event == '\r')
  {
    bdos_shell_submit_input();
    return;
  }

  // Backspace: remove the character immediately to the left of the cursor.
  if (key_event == '\b' || key_event == 127)
  {
    bdos_shell_backspace_char();
    bdos_shell_render_line();
    return;
  }

  // Delete: remove the character immediately to the right of the cursor.
  if (key_event == BDOS_KEY_DELETE)
  {
    bdos_shell_delete_char();
    bdos_shell_render_line();
    return;
  }

  // Left arrow: move the editing cursor one character to the left.
  if (key_event == BDOS_KEY_LEFT)
  {
    if (bdos_shell_cursor_index > 0)
    {
      bdos_shell_cursor_index--;
    }
    bdos_shell_render_line();
    return;
  }

  // Right arrow: move the editing cursor one character to the right.
  if (key_event == BDOS_KEY_RIGHT)
  {
    if (bdos_shell_cursor_index < bdos_shell_input_len)
    {
      bdos_shell_cursor_index++;
    }
    bdos_shell_render_line();
    return;
  }

  // Up arrow: recall an older command from the in-memory history.
  if (key_event == BDOS_KEY_UP)
  {
    bdos_shell_history_nav_up();
    bdos_shell_render_line();
    return;
  }

  // Down arrow: move toward newer history entries or restore current draft.
  if (key_event == BDOS_KEY_DOWN)
  {
    bdos_shell_history_nav_down();
    bdos_shell_render_line();
    return;
  }

  // Ctrl+C (ASCII 3): clear the current input line content.
  if (key_event == 3)
  {
    bdos_shell_input_len = 0;
    bdos_shell_cursor_index = 0;
    bdos_shell_input[0] = '\0';
    bdos_shell_input_overflow = 0;
    bdos_shell_history_nav_offset = -1;
    bdos_shell_render_line();
    return;
  }

  // Ctrl+L (ASCII 12): clear terminal and redraw a fresh prompt line.
  if (key_event == 12)
  {
    term_clear();
    bdos_shell_input_len = 0;
    bdos_shell_cursor_index = 0;
    bdos_shell_input[0] = '\0';
    bdos_shell_input_overflow = 0;
    bdos_shell_history_nav_offset = -1;
    bdos_shell_start_line();
    return;
  }

  // Printable ASCII: insert at cursor position (supports mid-line insertion).
  if (key_event >= 32 && key_event <= 126)
  {
    bdos_shell_insert_char((char)key_event);
    bdos_shell_render_line();
    return;
  }

  // Unhandled key events are ignored by the shell input editor.
}

// Initialize shell state and show startup banner.
void bdos_shell_init()
{
  term_set_palette(PALETTE_WHITE_ON_BLACK);
  term_clear();

  bdos_shell_input_len = 0;
  bdos_shell_cursor_index = 0;
  bdos_shell_input[0] = '\0';
  bdos_shell_input_overflow = 0;
  bdos_shell_last_render_len = 0;
  bdos_shell_cursor_visible = 0;
  bdos_shell_cursor_draw_x = 0;
  bdos_shell_cursor_draw_y = 0;
  bdos_shell_cursor_saved_tile = 0;
  bdos_shell_cursor_saved_palette = PALETTE_WHITE_ON_BLACK;
  bdos_shell_history_head = 0;
  bdos_shell_history_count = 0;
  bdos_shell_history_nav_offset = -1;
  bdos_shell_history_saved_input[0] = '\0';

  bdos_shell_start_micros = get_micros();

  bdos_shell_print_welcome();
  bdos_shell_on_startup();
  bdos_shell_start_line();
}

// Consume all queued keyboard events and feed shell editor.
void bdos_shell_tick()
{
  while (bdos_keyboard_event_available())
  {
    int key_event = bdos_keyboard_event_read();
    if (key_event != -1)
    {
      bdos_shell_handle_key_event(key_event);
    }
  }
}
