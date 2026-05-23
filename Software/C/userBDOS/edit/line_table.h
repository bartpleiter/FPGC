#ifndef LINE_TABLE_H
#define LINE_TABLE_H

#define LINE_TABLE_MAX_LINES  10000

typedef struct {
    int offsets[LINE_TABLE_MAX_LINES];
    int count;
} line_table_t;

line_table_t *line_table_create(void);
void          line_table_destroy(line_table_t *lt);
int           lt_line_count(line_table_t *lt);
int           lt_line_start(line_table_t *lt, int line);
int           lt_line_length(line_table_t *lt, int line, int content_len);
int           lt_offset_to_line(line_table_t *lt, int offset);
void          lt_build_from_buffer(line_table_t *lt, const unsigned char *buf, int len);
int           lt_insert_newline(line_table_t *lt, int offset);
int           lt_delete_newline(line_table_t *lt, int offset);
int           lt_adjust_offsets(line_table_t *lt, int from_offset, int delta);

#endif
