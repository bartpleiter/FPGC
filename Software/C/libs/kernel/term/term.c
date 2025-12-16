#include "libs/kernel/term/term.h"

// Internal state variables
unsigned int cursor_x = 0;        // Current column (0-39)
unsigned int cursor_y = 0;        // Current row (0-24)
unsigned int current_palette = 0; // Current palette index for text

// Shadow buffer to mirror screen content (needed for scrolling since GPU can't read back)
unsigned char screen_tiles[TERM_HEIGHT][TERM_WIDTH];
unsigned char screen_palettes[TERM_HEIGHT][TERM_WIDTH];

// Forward declarations of internal helper functions
void term_advance_cursor(void);
void term_newline(void);

// Initialize the terminal library
void term_init(void) {
    cursor_x = 0;
    cursor_y = 0;
    current_palette = 0;
    gpu_set_window_palette(0);
    term_clear();
}

// Clear the entire screen and reset cursor
void term_clear(void) {
    unsigned int x, y;
    
    // Clear shadow buffer and GPU screen
    for (y = 0; y < TERM_HEIGHT; y++) {
        for (x = 0; x < TERM_WIDTH; x++) {
            screen_tiles[y][x] = 0;
            screen_palettes[y][x] = 0;
            gpu_write_window_tile(x, y, 0, 0);
        }
    }
    
    cursor_x = 0;
    cursor_y = 0;
}

// Scroll screen content up by one line
void term_scroll(void) {
    unsigned int x, y;
    
    // Move all rows up by one
    for (y = 0; y < TERM_HEIGHT - 1; y++) {
        for (x = 0; x < TERM_WIDTH; x++) {
            screen_tiles[y][x] = screen_tiles[y + 1][x];
            screen_palettes[y][x] = screen_palettes[y + 1][x];
            gpu_write_window_tile(x, y, screen_tiles[y][x], screen_palettes[y][x]);
        }
    }
    
    // Clear the last row
    for (x = 0; x < TERM_WIDTH; x++) {
        screen_tiles[TERM_HEIGHT - 1][x] = 0;
        screen_palettes[TERM_HEIGHT - 1][x] = current_palette;
        gpu_write_window_tile(x, TERM_HEIGHT - 1, 0, current_palette);
    }
}

// Set cursor position (clamped to valid range)
void term_set_cursor(unsigned int x, unsigned int y) {
    // Clamp to valid range
    if (x >= TERM_WIDTH) x = TERM_WIDTH - 1;
    if (y >= TERM_HEIGHT) y = TERM_HEIGHT - 1;
    
    cursor_x = x;
    cursor_y = y;
}

// Get current cursor position
void term_get_cursor(unsigned int *x, unsigned int *y) {
    if (x != (unsigned int *)0) {
        *x = cursor_x;
    }
    if (y != (unsigned int *)0) {
        *y = cursor_y;
    }
}

// Internal helper: advance cursor by one position
void term_advance_cursor(void) {
    cursor_x++;
    if (cursor_x >= TERM_WIDTH) {
        cursor_x = 0;
        cursor_y++;
        if (cursor_y >= TERM_HEIGHT) {
            term_scroll();
            cursor_y = TERM_HEIGHT - 1;
        }
    }
}

// Internal helper: move cursor to start of next line
void term_newline(void) {
    cursor_x = 0;
    cursor_y++;
    if (cursor_y >= TERM_HEIGHT) {
        term_scroll();
        cursor_y = TERM_HEIGHT - 1;
    }
}

// Output a single character with special character handling
void term_putchar(char c) {
    // Handle special characters
    if (c == '\n') {  // Newline (0x0A)
        term_newline();
        return;
    } else if (c == '\r') {  // Carriage return (0x0D)
        cursor_x = 0;
        return;
    } else if (c == '\t') {  // Tab (0x09)
        // Calculate next tab stop
        cursor_x = (cursor_x + TAB_WIDTH) & ~(TAB_WIDTH - 1);
        if (cursor_x >= TERM_WIDTH) {
            term_newline();
        }
        return;
    } else if (c == '\b') {  // Backspace (0x08)
        if (cursor_x > 0) {
            cursor_x--;
        }
        return;
    }
    
    // For printable characters, write to screen and shadow buffer
    screen_tiles[cursor_y][cursor_x] = c;
    screen_palettes[cursor_y][cursor_x] = current_palette;
    gpu_write_window_tile(cursor_x, cursor_y, c, current_palette);
    
    term_advance_cursor();
}

// Output a null-terminated string
void term_puts(char *str) {
    if (str == (char *)0) {
        return;
    }
    
    while (*str != '\0') {
        term_putchar(*str);
        str++;
    }
}

// Output an integer as a string
void term_putint(int value)
{
    char buffer[12];
    itoa(value, buffer, 10);
    term_puts(buffer);
}

// Output an unsigned integer as a hexadecimal string, with optional "0x" prefix
void term_puthex(unsigned int value, int prefix)
{
    if (prefix) {
        term_puts("0x");
    }
    char buffer[9];
    itoa(value, buffer, 16);
    term_puts(buffer);
}

// Output a buffer of specified length
void term_write(char *buf, unsigned int len) {
    unsigned int i;
    
    if (buf == (char *)0) {
        return;
    }
    
    for (i = 0; i < len; i++) {
        term_putchar(buf[i]);
    }
}

// Set the palette index for subsequent character output
void term_set_palette(unsigned int palette_index) {
    current_palette = palette_index;
}
