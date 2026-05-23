#include <string.h>
#include <stdlib.h>
#include "gapbuf.h"

gapbuf_t *gapbuf_create(int initial_size)
{
    gapbuf_t *gb;
    if (initial_size < 256) initial_size = 256;
    gb = malloc(sizeof(gapbuf_t));
    if (!gb) return (gapbuf_t *)0;
    gb->buf = malloc(initial_size);
    if (!gb->buf) { free(gb); return (gapbuf_t *)0; }
    gb->capacity = initial_size;
    gb->gap_start = 0;
    gb->gap_end = initial_size;
    gb->content_len = 0;
    return gb;
}

void gapbuf_destroy(gapbuf_t *gb)
{
    if (!gb) return;
    if (gb->buf) free(gb->buf);
    free(gb);
}

int gapbuf_len(gapbuf_t *gb)
{
    return gb->content_len;
}

unsigned char gapbuf_at(gapbuf_t *gb, int pos)
{
    if (pos < 0 || pos >= gb->content_len) return 0;
    if (pos < gb->gap_start)
        return gb->buf[pos];
    return gb->buf[pos + (gb->gap_end - gb->gap_start)];
}

void gapbuf_move_to(gapbuf_t *gb, int pos)
{
    int gap_size;
    if (pos < 0) pos = 0;
    if (pos > gb->content_len) pos = gb->content_len;
    if (pos == gb->gap_start) return;
    gap_size = gb->gap_end - gb->gap_start;
    if (pos < gb->gap_start) {
        memmove(gb->buf + pos + gap_size, gb->buf + pos, gb->gap_start - pos);
    } else {
        memmove(gb->buf + gb->gap_start, gb->buf + gb->gap_end, pos - gb->gap_start);
    }
    gb->gap_start = pos;
    gb->gap_end = pos + gap_size;
}

int gapbuf_grow(gapbuf_t *gb)
{
    int new_cap;
    int gap_size;
    int after_gap;
    unsigned char *new_buf;

    new_cap = gb->capacity * 2;
    if (new_cap < gb->capacity + 4096) new_cap = gb->capacity + 4096;
    new_buf = malloc(new_cap);
    if (!new_buf) return -1;

    /* Copy pre-gap */
    if (gb->gap_start > 0)
        memcpy(new_buf, gb->buf, gb->gap_start);

    /* Copy post-gap to end of new buffer */
    after_gap = gb->capacity - gb->gap_end;
    gap_size = new_cap - gb->content_len;
    if (after_gap > 0)
        memcpy(new_buf + gb->gap_start + gap_size, gb->buf + gb->gap_end, after_gap);

    gb->gap_end = gb->gap_start + gap_size;
    gb->capacity = new_cap;
    free(gb->buf);
    gb->buf = new_buf;
    return 0;
}

void gapbuf_insert(gapbuf_t *gb, unsigned char ch)
{
    if (gb->gap_start == gb->gap_end) {
        if (gapbuf_grow(gb) != 0) return;
    }
    gb->buf[gb->gap_start] = ch;
    gb->gap_start++;
    gb->content_len++;
}

void gapbuf_insert_bytes(gapbuf_t *gb, const unsigned char *data, int len)
{
    int i;
    for (i = 0; i < len; i++) {
        if (gb->gap_start == gb->gap_end) {
            if (gapbuf_grow(gb) != 0) return;
        }
        gb->buf[gb->gap_start] = data[i];
        gb->gap_start++;
        gb->content_len++;
    }
}

unsigned char gapbuf_delete_before(gapbuf_t *gb)
{
    unsigned char ch;
    if (gb->gap_start == 0) return 0;
    gb->gap_start--;
    ch = gb->buf[gb->gap_start];
    gb->content_len--;
    return ch;
}

unsigned char gapbuf_delete_after(gapbuf_t *gb)
{
    unsigned char ch;
    if (gb->gap_end >= gb->capacity) return 0;
    ch = gb->buf[gb->gap_end];
    gb->gap_end++;
    gb->content_len--;
    return ch;
}

void gapbuf_load_content(gapbuf_t *gb, const unsigned char *data, int len)
{
    int gap_size;
    gb->gap_start = 0;
    gb->gap_end = gb->capacity;
    gb->content_len = 0;
    if (len > gb->capacity) len = gb->capacity;
    gap_size = gb->capacity - len;
    memcpy(gb->buf + gap_size, data, len);
    gb->gap_end = gap_size;
    gb->gap_start = 0;
    gb->content_len = len;
}

int gapbuf_save_content(gapbuf_t *gb, unsigned char *dst, int max_len)
{
    int pre, post, total;
    pre = gb->gap_start;
    post = gb->capacity - gb->gap_end;
    total = pre + post;
    if (total > max_len) total = max_len;

    if (pre > 0) {
        int n = pre;
        if (n > max_len) n = max_len;
        memcpy(dst, gb->buf, n);
    }
    if (post > 0 && pre < max_len) {
        int n = post;
        if (pre + n > max_len) n = max_len - pre;
        memcpy(dst + pre, gb->buf + gb->gap_end, n);
    }
    return total;
}
