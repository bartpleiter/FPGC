#include <stdlib.h>
#include "editor.h"

editor_t *editor_create(gapbuf_t *gb, line_table_t *lt)
{
    editor_t *ed;
    ed = malloc(sizeof(editor_t));
    if (!ed) return (editor_t *)0;
    ed->gb = gb;
    ed->lt = lt;
    ed->cursor_line = 0;
    ed->cursor_col = 0;
    ed->scroll_y = 0;
    ed->scroll_x = 0;
    ed->modified = 0;
    ed->lt_dirty = 0;
    ed->filepath[0] = '\0';
    ed->filename[0] = '\0';
    return ed;
}

void editor_destroy(editor_t *ed)
{
    if (ed) free(ed);
}

int editor_cursor_offset(editor_t *ed)
{
    return lt_line_start(ed->lt, ed->cursor_line) + ed->cursor_col;
}

int editor_cur_line_len(editor_t *ed)
{
    return lt_line_length(ed->lt, ed->cursor_line, gapbuf_len(ed->gb));
}

static void clamp_cursor_col(editor_t *ed)
{
    int len;
    len = editor_cur_line_len(ed);
    if (ed->cursor_col > len) ed->cursor_col = len;
}

void editor_move_left(editor_t *ed)
{
    if (ed->cursor_col > 0) {
        ed->cursor_col--;
    } else if (ed->cursor_line > 0) {
        ed->cursor_line--;
        ed->cursor_col = editor_cur_line_len(ed);
    }
}

void editor_move_right(editor_t *ed)
{
    int len;
    len = editor_cur_line_len(ed);
    if (ed->cursor_col < len) {
        ed->cursor_col++;
    } else if (ed->cursor_line < lt_line_count(ed->lt) - 1) {
        ed->cursor_line++;
        ed->cursor_col = 0;
    }
}

void editor_move_up(editor_t *ed)
{
    if (ed->cursor_line > 0) {
        ed->cursor_line--;
        clamp_cursor_col(ed);
    }
}

void editor_move_down(editor_t *ed)
{
    if (ed->cursor_line < lt_line_count(ed->lt) - 1) {
        ed->cursor_line++;
        clamp_cursor_col(ed);
    }
}

void editor_move_home(editor_t *ed)
{
    ed->cursor_col = 0;
}

void editor_move_end(editor_t *ed)
{
    ed->cursor_col = editor_cur_line_len(ed);
}

void editor_page_up(editor_t *ed)
{
    int i;
    for (i = 0; i < TEXT_ROWS; i++) {
        if (ed->cursor_line == 0) break;
        ed->cursor_line--;
    }
    clamp_cursor_col(ed);
}

void editor_page_down(editor_t *ed)
{
    int i;
    int max_line;
    max_line = lt_line_count(ed->lt) - 1;
    for (i = 0; i < TEXT_ROWS; i++) {
        if (ed->cursor_line >= max_line) break;
        ed->cursor_line++;
    }
    clamp_cursor_col(ed);
}

void editor_insert_char(editor_t *ed, unsigned char ch)
{
    int offset;
    offset = editor_cursor_offset(ed);
    gapbuf_move_to(ed->gb, offset);
    gapbuf_insert(ed->gb, ch);
    /* Non-newline: adjust offsets after insertion point */
    lt_adjust_offsets(ed->lt, offset, 1);
    ed->cursor_col++;
    ed->modified = 1;
}

void editor_insert_newline(editor_t *ed)
{
    int offset;
    offset = editor_cursor_offset(ed);
    gapbuf_move_to(ed->gb, offset);
    gapbuf_insert(ed->gb, '\n');
    if (lt_insert_newline(ed->lt, offset) == -1) {
        ed->lt_dirty = 1;
    }
    ed->cursor_line++;
    ed->cursor_col = 0;
    ed->modified = 1;
}

void editor_insert_tab(editor_t *ed)
{
    int i;
    int offset;
    offset = editor_cursor_offset(ed);
    gapbuf_move_to(ed->gb, offset);
    for (i = 0; i < 4; i++) {
        gapbuf_insert(ed->gb, ' ');
    }
    lt_adjust_offsets(ed->lt, offset, 4);
    ed->cursor_col += 4;
    ed->modified = 1;
}

void editor_backspace(editor_t *ed)
{
    int offset;
    unsigned char ch;
    offset = editor_cursor_offset(ed);
    if (offset == 0) return;
    gapbuf_move_to(ed->gb, offset);
    ch = gapbuf_delete_before(ed->gb);
    if (ch == '\n') {
        /* Merging with previous line */
        ed->cursor_line--;
        ed->cursor_col = editor_cur_line_len(ed);
        lt_delete_newline(ed->lt, offset - 1);
    } else {
        ed->cursor_col--;
        lt_adjust_offsets(ed->lt, offset - 1, -1);
    }
    ed->modified = 1;
}

void editor_delete(editor_t *ed)
{
    int offset;
    unsigned char ch;
    offset = editor_cursor_offset(ed);
    if (offset >= gapbuf_len(ed->gb)) return;
    gapbuf_move_to(ed->gb, offset);
    ch = gapbuf_delete_after(ed->gb);
    if (ch == '\n') {
        lt_delete_newline(ed->lt, offset);
    } else {
        lt_adjust_offsets(ed->lt, offset, -1);
    }
    ed->modified = 1;
}
