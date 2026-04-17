#ifndef FPGC_TERM_H
#define FPGC_TERM_H

/*
 * BDOS terminal API. Thin wrappers around libterm v2 (term2_*).
 *
 * Existing callers continue using term_*; for new v2 capabilities
 * (alt screen, ANSI escapes, line discipline, input events) include
 * <term2.h> directly.
 */

#define TERM_WIDTH         40
#define TERM_HEIGHT        25
#define TAB_WIDTH          4
#define TERM_HISTORY_LINES 200

void term_init(void);
void term_clear(void);
void term_scroll(void);
void term_set_cursor(unsigned int x, unsigned int y);
void term_get_cursor(unsigned int *x, unsigned int *y);
void term_putchar(char c);
void term_puts(const char *str);
void term_putint(int value);
void term_puthex(unsigned int value, int prefix);
void term_write(const char *buf, unsigned int len);
void term_get_cell(unsigned int x, unsigned int y,
                   unsigned char *tile, unsigned char *palette);
void term_put_cell(unsigned int x, unsigned int y,
                   unsigned char tile, unsigned char palette);
void term_set_palette(unsigned int palette_index);
int  term_scroll_view_up(void);
int  term_scroll_view_down(void);
int  term_is_scrolled_back(void);
void term_set_uart_mirror(int enable);

#endif /* FPGC_TERM_H */
#ifndef FPGC_TERM_H
#define FPGC_TERM_H

#define TERM_WIDTH   40
#define TERM_HEIGHT  25
#define TAB_WIDTH    4
#define TERM_HISTORY_LINES 200

void term_init(void);
void term_clear(void);
void term_scroll(void);
void term_set_cursor(unsigned int x, unsigned int y);
void term_get_cursor(unsigned int *x, unsigned int *y);
void term_putchar(char c);
void term_puts(const char *str);
void term_putint(int value);
void term_puthex(unsigned int value, int prefix);
void term_write(const char *buf, unsigned int len);
void term_get_cell(unsigned int x, unsigned int y, unsigned char *tile, unsigned char *palette);
void term_put_cell(unsigned int x, unsigned int y, unsigned char tile, unsigned char palette);
void term_set_palette(unsigned int palette_index);
int  term_scroll_view_up(void);
int  term_scroll_view_down(void);
int  term_is_scrolled_back(void);
void term_set_uart_mirror(int enable);

#endif /* FPGC_TERM_H */
