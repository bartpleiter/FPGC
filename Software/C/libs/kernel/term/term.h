#ifndef TERM_H
#define TERM_H

/*
 * Terminal Library
 * Provides text output capabilities on the window plane of the GPU.
 * Emulates basic terminal behavior.
 */

#define TERM_WIDTH  40
#define TERM_HEIGHT 25
#define TAB_WIDTH   4

// Initialize the terminal library
void term_init(void);

// Clear the entire screen and reset cursor
void term_clear(void);

// Scroll screen content up by one line
void term_scroll(void);

// Set cursor position (clamped to valid range)
void term_set_cursor(unsigned int x, unsigned int y);

// Get current cursor position
void term_get_cursor(unsigned int *x, unsigned int *y);

// Output a single character with special character handling
void term_putchar(char c);

// Output a null-terminated string
void term_puts(char *str);

// Output a buffer of specified length
void term_write(char *buf, unsigned int len);

// Set the palette index for subsequent character output
void term_set_palette(unsigned int palette_index);

#endif // TERM_H
