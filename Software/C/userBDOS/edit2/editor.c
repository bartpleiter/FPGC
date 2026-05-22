#include "editor.h"
#include <string.h>
#include <stdlib.h>

/* Convert cursor (line, col) to byte offset in the gap buffer. */
static int cursor_offset(editor_t *ed)
{
    return ed->cursor_col + lt_line_start(ed->lt, ed->cursor_line);
}

/* Clamp cursor column to the current line's length. */
static void clamp_cursor_col(editor_t *ed)
{
    int len = lt_line_length(ed->lt, ed->cursor_line, gapbuf_len(ed->gb));
    if (ed->cursor_col < 0) ed->cursor_col = 0;
    if (ed->cursor_col > len) ed->cursor_col = len;
}

/* Clamp cursor line to valid range. */
static void clamp_cursor_line(editor_t *ed)
{
    int lines = lt_line_count(ed->lt);
    if (ed->cursor_line < 0) ed->cursor_line = 0;
    if (ed->cursor_line >= lines) ed->cursor_line = lines - 1;
}

/* Create an editor state. The caller retains ownership of gb and lt. */
editor_t *editor_create(gapbuf_t *gb, line_table_t *lt)
{
    editor_t *ed = malloc(sizeof(editor_t));
    if (!ed) return NULL;

    ed->gb          = gb;
    ed->lt          = lt;
    ed->cursor_line = 0;
    ed->cursor_col  = 0;
    ed->scroll_y    = 0;
    ed->scroll_x    = 0;
    ed->modified    = 0;
    memset(ed->filepath, 0, sizeof(ed->filepath));
    memset(ed->filename, 0, sizeof(ed->filename));
    return ed;
}

/* Free the editor state (does NOT free gb or lt). */
void editor_destroy(editor_t *ed)
{
    free(ed);
}

/* --- Navigation --- */

void editor_move_left(editor_t *ed)
{
    if (ed->cursor_col > 0) {
        ed->cursor_col--;
    } else if (ed->cursor_line > 0) {
        /* Move to end of previous line. */
        ed->cursor_line--;
        clamp_cursor_line(ed);
        clamp_cursor_col(ed);
    }
}

void editor_move_right(editor_t *ed)
{
    int len = lt_line_length(ed->lt, ed->cursor_line, gapbuf_len(ed->gb));

    if (ed->cursor_line < lt_line_count(ed->lt) - 1) {
        /* Not last line: line ends with '\n'. cursor_col ranges [0..len].
           When at len (past last visible char), jump to next line. */
        if (ed->cursor_col < len) {
            ed->cursor_col++;
        } else {
            ed->cursor_line++;
            ed->cursor_col = 0;
        }
    } else {
        /* Last line: no '\n'. cursor_col ranges [0..len]. */
        if (ed->cursor_col < len) {
            ed->cursor_col++;
        }
    }
}

void editor_move_up(editor_t *ed)
{
    if (ed->cursor_line > 0) {
        ed->cursor_line--;
        clamp_cursor_line(ed);
        clamp_cursor_col(ed);
    }
}

void editor_move_down(editor_t *ed)
{
    if (ed->cursor_line < lt_line_count(ed->lt) - 1) {
        ed->cursor_line++;
        clamp_cursor_line(ed);
        clamp_cursor_col(ed);
    }
}

void editor_move_home(editor_t *ed)
{
    ed->cursor_col = 0;
}

void editor_move_end(editor_t *ed)
{
    ed->cursor_col = lt_line_length(ed->lt, ed->cursor_line, gapbuf_len(ed->gb));
}

void editor_page_up(editor_t *ed)
{
    ed->cursor_line -= EDITOR_TEXT_ROWS;
    clamp_cursor_line(ed);
    clamp_cursor_col(ed);
}

void editor_page_down(editor_t *ed)
{
    ed->cursor_line += EDITOR_TEXT_ROWS;
    clamp_cursor_line(ed);
    clamp_cursor_col(ed);
}

/* --- Edit operations --- */

void editor_insert_char(editor_t *ed, unsigned char ch)
{
    int offset = cursor_offset(ed);

    gapbuf_move_to(ed->gb, offset);
    gapbuf_insert(ed->gb, ch);

    ed->cursor_col++;
    ed->modified = 1;
}

void editor_insert_newline(editor_t *ed)
{
    int offset = cursor_offset(ed);

    gapbuf_move_to(ed->gb, offset);
    gapbuf_insert(ed->gb, '\n');
    lt_insert_newline(ed->lt, offset);

    ed->cursor_line++;
    ed->cursor_col = 0;
    ed->modified = 1;
}

void editor_insert_tab(editor_t *ed)
{
    int offset = cursor_offset(ed);
    gapbuf_move_to(ed->gb, offset);

    for (int i = 0; i < EDITOR_TAB_STOP; i++) {
        gapbuf_insert(ed->gb, ' ');
        ed->cursor_col++;
    }
    ed->modified = 1;
}

void editor_backspace(editor_t *ed)
{
    if (ed->cursor_col == 0 && ed->cursor_line > 0) {
        /* Merge with previous line: delete the '\n' at end of previous line. */
        int prev_line_end = lt_line_start(ed->lt, ed->cursor_line) - 1;

        gapbuf_move_to(ed->gb, prev_line_end);
        gapbuf_delete_after(ed->gb);
        lt_delete_newline(ed->lt, prev_line_end);

        /* Cursor moves to end of the now-merged previous line. */
        ed->cursor_line--;
        clamp_cursor_line(ed);
        clamp_cursor_col(ed);
        ed->modified = 1;
    } else if (ed->cursor_col > 0) {
        /* Delete character before cursor. */
        int offset = cursor_offset(ed);
        gapbuf_move_to(ed->gb, offset);
        gapbuf_delete_before(ed->gb);
        ed->cursor_col--;
        ed->modified = 1;
    }
}

void editor_delete(editor_t *ed)
{
    int len = lt_line_length(ed->lt, ed->cursor_line, gapbuf_len(ed->gb));

    if (ed->cursor_col >= len && ed->cursor_line < lt_line_count(ed->lt) - 1) {
        /* At end of non-last line: merge with next line by deleting '\n'. */
        int nl_pos = lt_line_start(ed->lt, ed->cursor_line) + len;

        gapbuf_move_to(ed->gb, nl_pos);
        gapbuf_delete_after(ed->gb);
        lt_delete_newline(ed->lt, nl_pos);
        ed->modified = 1;
    } else if (ed->cursor_col < len) {
        /* Delete character after cursor. */
        int offset = cursor_offset(ed);
        gapbuf_move_to(ed->gb, offset);
        gapbuf_delete_after(ed->gb);
        ed->modified = 1;
    }
}

/* --- State queries --- */

int editor_is_modified(editor_t *ed)
{
    return ed->modified;
}

void editor_set_modified(editor_t *ed, int val)
{
    ed->modified = val;
}
