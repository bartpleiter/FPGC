#include "fileio.h"
#include <syscall.h>
#include <string.h>

#define CHUNK_SIZE 4096

int file_load(editor_t *ed, const char *path)
{
    int fd = sys_open(path, O_RDONLY);
    if (fd < 0) return -1;

    unsigned char chunk[CHUNK_SIZE];
    int total = 0;

    /* Initialize line table with first line offset. */
    ed->lt->count = 1;
    ed->lt->offsets[0] = 0;

    while (1)
    {
        int n = sys_read(fd, chunk, CHUNK_SIZE);
        if (n <= 0) break;

        /* Record newline positions in this chunk for line table. */
        for (int i = 0; i < n; i++)
        {
            if (chunk[i] == '\n')
            {
                if (ed->lt->count < LINE_TABLE_MAX_LINES)
                {
                    ed->lt->offsets[ed->lt->count++] = total + i + 1;
                }
                /* Line table full — continue loading but line table will be
                   incomplete; editor falls back to O(n) scan. */
            }
        }

        /* Append chunk to gap buffer at end. */
        gapbuf_move_to(ed->gb, ed->gb->content_len);
        for (int i = 0; i < n; i++)
        {
            gapbuf_insert(ed->gb, chunk[i]);
        }
        total += n;
    }

    sys_close(fd);

    /* Extract filename from path. */
    const char *name = path;
    for (const char *p = path; *p; p++) { if (*p == '/') name = p + 1; }
    int len = 0;
    while (*name && len < 19) { ed->filename[len++] = *name++; }
    ed->filename[len] = 0;

    /* Copy full path. */
    int pl = 0;
    while (path[pl] && pl < 127) { ed->filepath[pl] = path[pl++]; }
    ed->filepath[pl] = 0;

    ed->cursor_line = 0;
    ed->cursor_col = 0;
    ed->modified = 0;

    return 0;
}

int file_save(editor_t *ed, const char *path)
{
    int fd = sys_open(path, O_WRONLY | O_CREAT | O_TRUNC);
    if (fd < 0) return -1;

    int total = gapbuf_len(ed->gb);
    unsigned char *buf = ed->gb->buf;
    int gs = ed->gb->gap_start;
    int ge = ed->gb->gap_end;

    /* Write pre-gap content. */
    if (gs > 0)
    {
        sys_write(fd, buf, gs);
    }

    /* Write post-gap content. */
    int remaining = total - gs;
    if (remaining > 0)
    {
        sys_write(fd, buf + ge, remaining);
    }

    sys_close(fd);
    ed->modified = 0;
    return 0;
}
