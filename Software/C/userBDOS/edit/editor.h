#ifndef EDITOR_H
#define EDITOR_H

#include "gapbuf.h"
#include "line_table.h"

#define SCREEN_WIDTH   40
#define SCREEN_HEIGHT  25
#define TEXT_ROWS      23   /* Rows 1..23 for file content */
#define HEADER_ROW     0
#define STATUS_ROW     24

typedef struct {
    gapbuf_t     *gb;
    line_table_t *lt;
    int           cursor_line;
    int           cursor_col;
    int           scroll_y;
    int           scroll_x;
    int           modified;
    int           lt_dirty;
    char          filepath[128];
    char          filename[20];
} editor_t;

editor_t *editor_create(gapbuf_t *gb, line_table_t *lt);
void      editor_destroy(editor_t *ed);
void      editor_move_left(editor_t *ed);
void      editor_move_right(editor_t *ed);
void      editor_move_up(editor_t *ed);
void      editor_move_down(editor_t *ed);
void      editor_move_home(editor_t *ed);
void      editor_move_end(editor_t *ed);
void      editor_page_up(editor_t *ed);
void      editor_page_down(editor_t *ed);
void      editor_insert_char(editor_t *ed, unsigned char ch);
void      editor_insert_newline(editor_t *ed);
void      editor_insert_tab(editor_t *ed);
void      editor_backspace(editor_t *ed);
void      editor_delete(editor_t *ed);
int       editor_cursor_offset(editor_t *ed);
int       editor_cur_line_len(editor_t *ed);

#endif
