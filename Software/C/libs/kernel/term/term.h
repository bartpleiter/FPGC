#ifndef TERM_H
#define TERM_H

// Terminal Library
// Provides text output capabilities on the window plane of the GPU.
// Emulates basic terminal behavior.

#define TERM_WIDTH 40
#define TERM_HEIGHT 25
#define TAB_WIDTH 4
#define TERM_HISTORY_LINES 200  // Number of scrollback history lines

// Initialize the terminal library
void term_init();

// Clear the entire screen and reset cursor
void term_clear();

// Scroll screen content up by one line
void term_scroll();

// Set cursor position (clamped to valid range)
void term_set_cursor(unsigned int x, unsigned int y);

// Get current cursor position
void term_get_cursor(unsigned int *x, unsigned int *y);

// Output a single character with special character handling
void term_putchar(char c);

// Output a null-terminated string
void term_puts(char *str);

// Output an integer as a string
void term_putint(int value);

// Output an unsigned integer as a hexadecimal string, with optional "0x" prefix
void term_puthex(unsigned int value, int prefix);

// Output a buffer of specified length
void term_write(char *buf, unsigned int len);

// Read tile and palette at a terminal cell
void term_get_cell(unsigned int x, unsigned int y, unsigned char *tile, unsigned char *palette);

// Write tile and palette at a terminal cell without moving cursor
void term_put_cell(unsigned int x, unsigned int y, unsigned char tile, unsigned char palette);

// Set the palette index for subsequent character output
void term_set_palette(unsigned int palette_index);

// Scroll the view up (back into history). Returns 1 if scrolled, 0 if at limit.
int term_scroll_view_up();

// Scroll the view down (towards current output). Returns 1 if scrolled, 0 if already at bottom.
int term_scroll_view_down();

// Returns 1 if the terminal is currently scrolled back into history
int term_is_scrolled_back();

#endif // TERM_H
