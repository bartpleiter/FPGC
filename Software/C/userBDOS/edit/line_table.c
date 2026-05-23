#include <stdlib.h>
#include <string.h>
#include "line_table.h"

line_table_t *line_table_create(void)
{
    line_table_t *lt;
    lt = malloc(sizeof(line_table_t));
    if (!lt) return (line_table_t *)0;
    lt->count = 0;
    return lt;
}

void line_table_destroy(line_table_t *lt)
{
    if (lt) free(lt);
}

int lt_line_count(line_table_t *lt)
{
    return lt->count;
}

int lt_line_start(line_table_t *lt, int line)
{
    if (line < 0) return 0;
    if (line >= lt->count) return lt->offsets[lt->count - 1];
    return lt->offsets[line];
}

int lt_line_length(line_table_t *lt, int line, int content_len)
{
    int start, end;
    if (line < 0 || line >= lt->count) return 0;
    start = lt->offsets[line];
    if (line + 1 < lt->count)
        end = lt->offsets[line + 1] - 1; /* exclude the \n */
    else
        end = content_len;
    if (end < start) return 0;
    return end - start;
}

int lt_offset_to_line(line_table_t *lt, int offset)
{
    int lo, hi, mid;
    if (lt->count <= 0) return 0;
    lo = 0;
    hi = lt->count - 1;
    while (lo < hi) {
        mid = lo + (hi - lo + 1) / 2;
        if (lt->offsets[mid] <= offset) lo = mid;
        else hi = mid - 1;
    }
    return lo;
}

void lt_build_from_buffer(line_table_t *lt, const unsigned char *buf, int len)
{
    int i;
    lt->count = 0;
    /* First line always starts at offset 0 */
    if (lt->count < LINE_TABLE_MAX_LINES) {
        lt->offsets[lt->count] = 0;
        lt->count++;
    }
    for (i = 0; i < len; i++) {
        if (buf[i] == '\n' && lt->count < LINE_TABLE_MAX_LINES) {
            lt->offsets[lt->count] = i + 1;
            lt->count++;
        }
    }
}

int lt_insert_newline(line_table_t *lt, int offset)
{
    int line, insert_pos;
    if (lt->count >= LINE_TABLE_MAX_LINES) return -1;

    /* Find which line this offset belongs to */
    line = lt_offset_to_line(lt, offset);
    insert_pos = line + 1;

    /* Shift offsets after insert_pos up by 1 element */
    if (insert_pos < lt->count) {
        memmove(&lt->offsets[insert_pos + 1], &lt->offsets[insert_pos],
                (lt->count - insert_pos) * sizeof(int));
    }

    /* New line starts at offset + 1 (char after the newline) */
    lt->offsets[insert_pos] = offset + 1;
    lt->count++;

    /* Adjust offsets after the new line by +1 (for the inserted \n char) */
    {
        int i;
        for (i = insert_pos + 1; i < lt->count; i++)
            lt->offsets[i]++;
    }
    return 0;
}

int lt_delete_newline(line_table_t *lt, int offset)
{
    int line, remove_pos, i;
    if (lt->count <= 1) return -1;

    /* The newline at 'offset' separates line and line+1.
       We need to remove the line+1 entry. */
    line = lt_offset_to_line(lt, offset);
    remove_pos = line + 1;
    if (remove_pos >= lt->count) return -1;

    /* Remove the entry */
    if (remove_pos + 1 < lt->count) {
        memmove(&lt->offsets[remove_pos], &lt->offsets[remove_pos + 1],
                (lt->count - remove_pos - 1) * sizeof(int));
    }
    lt->count--;

    /* Adjust offsets after the merge point by -1 */
    for (i = remove_pos; i < lt->count; i++)
        lt->offsets[i]--;
    return 0;
}

int lt_adjust_offsets(line_table_t *lt, int from_offset, int delta)
{
    int i;
    for (i = 0; i < lt->count; i++) {
        if (lt->offsets[i] > from_offset)
            lt->offsets[i] += delta;
    }
    return 0;
}
