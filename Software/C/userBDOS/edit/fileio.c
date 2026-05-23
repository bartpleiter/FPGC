#include <string.h>
#include <syscall.h>
#include "fileio.h"
#include "gapbuf.h"
#include "line_table.h"

#define CHUNK_SIZE 4096

int file_load(editor_t *ed, const char *path)
{
    int fd, fsize, alloc_size, n, total;
    unsigned char chunk[CHUNK_SIZE];

    /* Store path */
    {
        int i;
        for (i = 0; path[i] && i < 127; i++)
            ed->filepath[i] = path[i];
        ed->filepath[i] = '\0';
    }

    /* Extract filename from path */
    {
        const char *p;
        int i;
        p = path;
        /* Find last '/' */
        for (i = 0; path[i]; i++) {
            if (path[i] == '/') p = &path[i + 1];
        }
        for (i = 0; p[i] && i < 19; i++)
            ed->filename[i] = p[i];
        ed->filename[i] = '\0';
    }

    fd = sys_open(path, 1 /* O_RDONLY */);
    if (fd < 0) {
        /* New file — initialize empty buffer */
        gapbuf_destroy(ed->gb);
        ed->gb = gapbuf_create(4096);
        if (!ed->gb) return -1;
        lt_build_from_buffer(ed->lt, (const unsigned char *)"", 0);
        ed->cursor_line = 0;
        ed->cursor_col = 0;
        ed->modified = 0;
        return 0;
    }

    fsize = sys_lseek(fd, 0, 2 /* SEEK_END */);
    if (fsize < 0) fsize = 0;
    sys_lseek(fd, 0, 0 /* SEEK_SET */);

    /* Allocate gap buffer: file size + 25% headroom, min 4KB gap */
    {
        int gap = fsize / 4;
        if (gap < 4096) gap = 4096;
        alloc_size = fsize + gap;
    }

    gapbuf_destroy(ed->gb);
    ed->gb = gapbuf_create(alloc_size);
    if (!ed->gb) {
        sys_close(fd);
        return -1;
    }

    /* Read file in chunks directly into gap buffer */
    total = 0;
    while (1) {
        n = sys_read(fd, chunk, CHUNK_SIZE);
        if (n <= 0) break;
        gapbuf_move_to(ed->gb, total);
        gapbuf_insert_bytes(ed->gb, chunk, n);
        total += n;
    }
    sys_close(fd);

    /* Build line table from gap buffer content */
    {
        /* We need a linearized copy for lt_build_from_buffer.
           For efficiency, scan the gap buffer directly. */
        int i;
        ed->lt->count = 0;
        /* First line always starts at 0 */
        if (ed->lt->count < LINE_TABLE_MAX_LINES) {
            ed->lt->offsets[ed->lt->count] = 0;
            ed->lt->count++;
        }
        for (i = 0; i < total; i++) {
            if (gapbuf_at(ed->gb, i) == '\n' && ed->lt->count < LINE_TABLE_MAX_LINES) {
                ed->lt->offsets[ed->lt->count] = i + 1;
                ed->lt->count++;
            }
        }
    }

    ed->cursor_line = 0;
    ed->cursor_col = 0;
    ed->scroll_y = 0;
    ed->scroll_x = 0;
    ed->modified = 0;
    ed->lt_dirty = 0;
    return 0;
}

int file_save(editor_t *ed)
{
    int fd, total, written, chunk_len;
    unsigned char chunk[CHUNK_SIZE];
    int content_len;

    if (ed->filepath[0] == '\0') return -1;

    fd = sys_open(ed->filepath, 2 | 8 | 16 /* O_WRONLY | O_CREAT | O_TRUNC */);
    if (fd < 0) return -1;

    content_len = gapbuf_len(ed->gb);
    total = 0;
    while (total < content_len) {
        chunk_len = content_len - total;
        if (chunk_len > CHUNK_SIZE) chunk_len = CHUNK_SIZE;
        {
            int i;
            for (i = 0; i < chunk_len; i++)
                chunk[i] = gapbuf_at(ed->gb, total + i);
        }
        written = sys_write(fd, chunk, chunk_len);
        if (written <= 0) {
            sys_close(fd);
            return -1;
        }
        total += written;
    }

    sys_close(fd);
    ed->modified = 0;
    return 0;
}
