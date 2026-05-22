#ifndef GAPBUF_H
#define GAPBUF_H

typedef struct {
    unsigned char *buf;
    int gap_start;
    int gap_end;
    int capacity;
    int content_len;
} gapbuf_t;

gapbuf_t *gapbuf_create(int initial_size);
void      gapbuf_destroy(gapbuf_t *gb);
int       gapbuf_len(gapbuf_t *gb);
unsigned char gapbuf_at(gapbuf_t *gb, int pos);
void      gapbuf_move_to(gapbuf_t *gb, int pos);
void      gapbuf_insert(gapbuf_t *gb, unsigned char ch);
unsigned char gapbuf_delete_before(gapbuf_t *gb);
unsigned char gapbuf_delete_after(gapbuf_t *gb);
int       gapbuf_grow(gapbuf_t *gb);
void      gapbuf_load_content(gapbuf_t *gb, const unsigned char *data, int len);
void      gapbuf_save_content(gapbuf_t *gb, unsigned char *dst);

#endif
