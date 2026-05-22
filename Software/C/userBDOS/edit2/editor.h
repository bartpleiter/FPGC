#ifndef EDITOR_H
#define EDITOR_H

#include "gapbuf.h"
#include "line_table.h"

#define EDITOR_TAB_STOP 4
#define EDITOR_TEXT_ROWS 23

typedef struct {
    gapbuf_t     *gb;
    line_table_t *lt;
    int           cursor_line;
    int           cursor_col;
    int           scroll_y;
    int           scroll_x;
    int           modified;
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
void      ensure_cursor_visible(editor_t *ed);
int       editor_is_modified(editor_t *ed);
void      editor_set_modified(editor_t *ed, int val);

#endif
