#include "line_table.h"
#include <stdlib.h>
#include <string.h>

/* Allocate and zero-initialize a line table. */
line_table_t *line_table_create(void)
{
    line_table_t *lt = malloc(sizeof(line_table_t));
    if (!lt) return NULL;
    lt->count = 0;
    return lt;
}

/* Free the line table. */
void line_table_destroy(line_table_t *lt)
{
    free(lt);
}

/* Return the number of lines tracked. */
int lt_line_count(line_table_t *lt)
{
    return lt->count;
}

/* Return the byte offset where `line` starts. Clamps to valid range. */
int lt_line_start(line_table_t *lt, int line)
{
    if (line < 0) line = 0;
    if (line >= lt->count) line = lt->count - 1;
    return lt->offsets[line];
}

/* Return the length of `line` in bytes (including '\n' if present). */
int lt_line_length(line_table_t *lt, int line, int content_len)
{
    if (line < 0 || line >= lt->count) return 0;

    if (line == lt->count - 1) {
        /* Last line: extends to end of content. */
        return content_len - lt->offsets[line];
    }
    /* All other lines end with '\n'; subtract the newline itself. */
    return lt->offsets[line + 1] - lt->offsets[line] - 1;
}

/* Binary search: find the largest `i` such that offsets[i] <= offset. */
int lt_offset_to_line(line_table_t *lt, int offset)
{
    int lo = 0, hi = lt->count - 1;
    while (lo < hi) {
        int mid = lo + (hi - lo + 1) / 2;
        if (lt->offsets[mid] <= offset) {
            lo = mid;
        } else {
            hi = mid - 1;
        }
    }
    return lo;
}

/* Build the line table by scanning `buf` for '\n' characters. */
void lt_build_from_buffer(line_table_t *lt, const unsigned char *buf, int len)
{
    lt->count = 0;

    if (len <= 0) {
        /* Empty file: one empty line at offset 0. */
        lt->offsets[0] = 0;
        lt->count = 1;
        return;
    }

    /* First line always starts at 0. */
    lt->offsets[lt->count++] = 0;

    for (int i = 0; i < len; i++) {
        if (buf[i] == '\n') {
            /* Next line starts right after the newline. */
            if (lt->count >= LINE_TABLE_MAX_LINES) break;
            lt->offsets[lt->count++] = i + 1;
        }
    }

    /* If content doesn't end with '\n', we already have the last line start
       recorded from the loop above (or from the initial 0). If it does end
       with '\n', the last entry points past EOF — that's fine, it represents
       an empty trailing line. */

    /* Ensure at least one line exists. */
    if (lt->count == 0) {
        lt->offsets[0] = 0;
        lt->count = 1;
    }
}

/* Insert non-newline bytes at `offset`. No line-table change needed. */
int lt_insert(line_table_t *lt, int offset, int count)
{
    (void)lt; (void)offset; (void)count;
    return 0;
}

/* Delete non-newline bytes at `offset`. No line-table change needed. */
int lt_delete(line_table_t *lt, int offset, int count)
{
    (void)lt; (void)offset; (void)count;
    return 0;
}

/* Record that a '\n' was inserted at byte position `offset`. All subsequent
   line starts shift by +1, and a new line entry is added. */
int lt_insert_newline(line_table_t *lt, int offset)
{
    if (lt->count >= LINE_TABLE_MAX_LINES) return -1;

    /* Find which line contains `offset`. */
    int line = lt_offset_to_line(lt, offset);

    /* The new line starts at offset + 1 (right after the '\n'). */
    int new_offset = offset + 1;

    /* Shift all offsets after `line` by +1 to account for the inserted byte. */
    for (int i = lt->count - 1; i > line; i--) {
        lt->offsets[i]++;
    }

    /* Insert the new line entry at position line+1. */
    memmove(lt->offsets + line + 2,
            lt->offsets + line + 1,
            (lt->count - line - 1) * sizeof(int));
    lt->offsets[line + 1] = new_offset;
    lt->count++;

    return 0;
}

/* Record that a '\n' at byte position `offset` was deleted. The line containing
   `offset` is merged with the next line. */
int lt_delete_newline(line_table_t *lt, int offset)
{
    if (lt->count <= 1) return -1;

    /* Find which line contains `offset`. */
    int line = lt_offset_to_line(lt, offset);

    /* The next line's entry is at index line+1 — remove it. */
    if (line + 1 >= lt->count) return -1;

    /* Remove offsets[line+1] and shift remaining entries left. */
    memmove(lt->offsets + line + 1,
            lt->offsets + line + 2,
            (lt->count - line - 2) * sizeof(int));
    lt->count--;

    /* Shift all offsets after `line` by -1 to account for the removed byte. */
    for (int i = lt->count; i > line; i--) {
        lt->offsets[i]--;
    }

    return 0;
}
