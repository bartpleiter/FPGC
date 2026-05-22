#include "gapbuf.h"
#include <stdlib.h>
#include <string.h>

/* Create a gap buffer with the given initial capacity. */
gapbuf_t *gapbuf_create(int initial_size)
{
    gapbuf_t *gb = malloc(sizeof(gapbuf_t));
    if (!gb) return NULL;

    gb->buf = malloc(initial_size);
    if (!gb->buf) {
        free(gb);
        return NULL;
    }

    gb->gap_start   = 0;
    gb->gap_end     = initial_size;
    gb->capacity    = initial_size;
    gb->content_len = 0;
    return gb;
}

/* Free the gap buffer and its backing store. */
void gapbuf_destroy(gapbuf_t *gb)
{
    if (!gb) return;
    free(gb->buf);
    free(gb);
}

/* Return the logical length of content (excluding the gap). */
int gapbuf_len(gapbuf_t *gb)
{
    return gb->content_len;
}

/* Read a byte at logical position `pos`. The caller must ensure 0 <= pos < len. */
unsigned char gapbuf_at(gapbuf_t *gb, int pos)
{
    if (pos < gb->gap_start) {
        return gb->buf[pos];
    }
    return gb->buf[pos + (gb->gap_end - gb->gap_start)];
}

/* Move the gap so its start is at logical position `pos`. */
void gapbuf_move_to(gapbuf_t *gb, int pos)
{
    if (pos == gb->gap_start) return;

    int gap_size = gb->gap_end - gb->gap_start;

    if (pos < gb->gap_start) {
        /* Gap moves left: shift content [pos .. gap_start-1] right by gap_size. */
        memmove(gb->buf + pos + gap_size, gb->buf + pos,
                gb->gap_start - pos);
    } else {
        /* pos > gb->gap_end (or pos in gap): shift content [gap_end .. pos-1] left. */
        memmove(gb->buf + gb->gap_start, gb->buf + gb->gap_end,
                pos - gb->gap_end);
    }

    gb->gap_start = pos;
    gb->gap_end   = pos + gap_size;
}

/* Insert a single byte at the current gap position. Grows if gap collapses. */
void gapbuf_insert(gapbuf_t *gb, unsigned char ch)
{
    gb->buf[gb->gap_start] = ch;
    gb->gap_start++;
    gb->content_len++;

    if (gb->gap_start == gb->gap_end) {
        gapbuf_grow(gb);
    }
}

/* Delete the byte immediately before the gap. Returns the deleted byte, or 0
   if the gap is already at position 0. */
unsigned char gapbuf_delete_before(gapbuf_t *gb)
{
    if (gb->gap_start == 0) return 0;

    gb->gap_start--;
    unsigned char ch = gb->buf[gb->gap_start];
    gb->content_len--;
    return ch;
}

/* Delete the byte immediately after the gap. Returns the deleted byte. */
unsigned char gapbuf_delete_after(gapbuf_t *gb)
{
    unsigned char ch = gb->buf[gb->gap_end];
    gb->gap_end++;
    gb->content_len--;
    return ch;
}

/* Double the buffer capacity. Returns 0 on success, -1 if allocation fails. */
int gapbuf_grow(gapbuf_t *gb)
{
    int new_capacity = gb->capacity * 2;
    unsigned char *new_buf = malloc(new_capacity);
    if (!new_buf) return -1;

    int gap_size = gb->gap_end - gb->gap_start;

    /* Copy pre-gap content to the start of the new buffer. */
    memmove(new_buf, gb->buf, gb->gap_start);
    /* Copy post-gap content right after where the gap will be. */
    memmove(new_buf + gb->gap_start + gap_size,
            gb->buf + gb->gap_end,
            gb->capacity - gb->gap_end);

    free(gb->buf);
    gb->buf      = new_buf;
    gb->gap_end  = gb->gap_start + gap_size;
    gb->capacity = new_capacity;
    return 0;
}

/* Load external data into the buffer. The gap is moved to the end first, then
   bytes are inserted. */
void gapbuf_load_content(gapbuf_t *gb, const unsigned char *data, int len)
{
    /* Move gap to end of existing content. */
    gapbuf_move_to(gb, gb->content_len);

    /* Insert bytes in bulk if there's enough room, otherwise one at a time. */
    int gap_size = gb->gap_end - gb->gap_start;
    int batch = len < gap_size ? len : gap_size;

    if (batch > 0) {
        memmove(gb->buf + gb->gap_start, data, batch);
        gb->gap_start += batch;
        gb->content_len += batch;
        data += batch;
        len -= batch;
    }

    while (len > 0) {
        gapbuf_insert(gb, *data++);
        len--;
    }
}

/* Write the logical content (with no gap) into `dst`. The caller must ensure
   dst is at least gapbuf_len(gb) bytes. */
void gapbuf_save_content(gapbuf_t *gb, unsigned char *dst)
{
    if (gb->gap_start > 0) {
        memmove(dst, gb->buf, gb->gap_start);
    }
    int post_gap = gb->content_len - gb->gap_start;
    if (post_gap > 0) {
        memmove(dst + gb->gap_start, gb->buf + gb->gap_end,
                post_gap);
    }
}
