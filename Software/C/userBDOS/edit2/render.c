#include "render.h"
#include <syscall.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Constants                                                           */
/* ------------------------------------------------------------------ */

#define SCREEN_WIDTH   40
#define SCREEN_HEIGHT  25
#define TEXT_ROWS      23
#define HEADER_ROW     0
#define STATUS_ROW     24

#define PAL_DEFAULT    0
#define PAL_HEADER     1
#define PAL_STATUS     28
#define PAL_CURSOR     1
#define PAL_LINENUM    14

/* ------------------------------------------------------------------ */
/* Shadow buffer                                                       */
/* ------------------------------------------------------------------ */

struct shadow_cell {
    unsigned char tile;     /* last-rendered character (0 = uninitialized) */
    unsigned char palette;  /* last-rendered palette index                */
};

static struct shadow_cell shadow[SCREEN_HEIGHT][SCREEN_WIDTH];
static int shadow_initialized = 0;
static int status_bar_rendered = 0;

/* ------------------------------------------------------------------ */
/* Output buffer (coalescing writes)                                   */
/* ------------------------------------------------------------------ */

#define OUT_BUF_SIZE 8192
static char out_buf[OUT_BUF_SIZE];
static int  out_buf_n = 0;

static void out_flush(void)
{
    if (out_buf_n > 0) {
        sys_write(1, out_buf, out_buf_n);
        out_buf_n = 0;
    }
}

static void out_write(const char *s, int n)
{
    int i;
    if (n <= 0) return;
    if (n >= OUT_BUF_SIZE) { out_flush(); sys_write(1, (char *)s, n); return; }
    if (out_buf_n + n > OUT_BUF_SIZE) out_flush();
    for (i = 0; i < n; i++) out_buf[out_buf_n + i] = s[i];
    out_buf_n += n;
}

/* ------------------------------------------------------------------ */
/* ANSI helpers                                                        */
/* ------------------------------------------------------------------ */

static int last_palette = -1;

static int ansi_strlen(const char *s)
{
    int n = 0;
    while (s[n] != 0) n++;
    return n;
}

static void ansi_write(const char *s)
{
    out_write(s, ansi_strlen(s));
}

static void ansi_emit_uint(int v, char *dst, int *pos)
{
    char tmp[12];
    int n = 0;
    if (v == 0) { dst[(*pos)++] = '0'; return; }
    while (v > 0) { tmp[n++] = (char)('0' + (v % 10)); v /= 10; }
    while (n > 0) { dst[(*pos)++] = tmp[--n]; }
}

/* Move cursor to (x, y) in 0-based coordinates. */
static void ansi_goto(int x, int y)
{
    char buf[16];
    int n = 0;
    buf[n++] = 0x1B; buf[n++] = '[';
    ansi_emit_uint(y + 1, buf, &n);
    buf[n++] = ';';
    ansi_emit_uint(x + 1, buf, &n);
    buf[n++] = 'H';
    out_write(buf, n);
}

/* Emit an SGR that sets the tile palette directly. */
static void ansi_set_palette(int pal)
{
    char buf[16];
    int n;

    if (pal == last_palette) return;
    last_palette = pal;

    n = 0;
    buf[n++] = 0x1B; buf[n++] = '[';
    if (pal == 0) {
        buf[n++] = '0';
    } else {
        buf[n++] = '3'; buf[n++] = '8'; buf[n++] = ';';
        buf[n++] = '5'; buf[n++] = ';';
        ansi_emit_uint(pal & 0xFF, buf, &n);
    }
    buf[n++] = 'm';
    out_write(buf, n);
}

/* ------------------------------------------------------------------ */
/* Shadow helpers                                                      */
/* ------------------------------------------------------------------ */

static void shadow_invalidate_row(int y)
{
    int x;
    for (x = 0; x < SCREEN_WIDTH; x++)
        shadow[y][x].tile = 0;
}

static void shadow_invalidate_text_area(void)
{
    int y;
    for (y = 1; y <= TEXT_ROWS; y++)
        shadow_invalidate_row(y);
}

static void shadow_update(int x, int y, unsigned char ch, unsigned char pal)
{
    shadow[y][x].tile = ch;
    shadow[y][x].palette = pal;
}

/* ------------------------------------------------------------------ */
/* Shadow scroll operations                                            */
/* ------------------------------------------------------------------ */

static void shadow_scroll_down(int rows)
{
    int y;
    for (y = TEXT_ROWS; y >= rows + 1; y--)
        memmove(&shadow[y], &shadow[y - rows], sizeof(shadow[0]));
    for (y = 1; y < 1 + rows; y++)
        shadow_invalidate_row(y);
}

static void shadow_scroll_up(int rows)
{
    int y;
    for (y = 1; y <= TEXT_ROWS - rows; y++)
        memmove(&shadow[y], &shadow[y + rows], sizeof(shadow[0]));
    for (y = TEXT_ROWS - rows + 1; y <= TEXT_ROWS; y++)
        shadow_invalidate_row(y);
}

/* ------------------------------------------------------------------ */
/* Row-level batch rendering                                           */
/* ------------------------------------------------------------------ */

static void render_text_row(editor_t *ed, int screen_y, int doc_line)
{
    unsigned char chars[SCREEN_WIDTH];
    unsigned char palettes[SCREEN_WIDTH];
    int x;

    /* 1. Extract row content into local buffers. */
    {
        int line_start = lt_line_start(ed->lt, doc_line);
        int line_len = lt_line_length(ed->lt, doc_line, gapbuf_len(ed->gb));
        for (x = 0; x < SCREEN_WIDTH; x++) {
            int col = x + ed->scroll_x;
            if (col >= line_len) {
                chars[x] = ' ';
                palettes[x] = PAL_DEFAULT;
            } else {
                chars[x] = gapbuf_at(ed->gb, line_start + col);
                palettes[x] = PAL_DEFAULT;
            }
        }
    }

    /* 2. Highlight cursor cell. */
    {
        int cx = ed->cursor_col - ed->scroll_x;
        if (doc_line == ed->cursor_line && cx >= 0 && cx < SCREEN_WIDTH) {
            palettes[cx] = PAL_CURSOR;
        }
    }

    /* 3. Compare against shadow — skip if identical. */
    {
        int dirty = 0;
        for (x = 0; x < SCREEN_WIDTH; x++) {
            if (chars[x] != shadow[screen_y][x].tile ||
                palettes[x] != shadow[screen_y][x].palette) {
                dirty = 1;
                break;
            }
        }
        if (!dirty) return;
    }

    /* 4. Batch render: one goto, then palette-grouped writes. */
    ansi_goto(0, screen_y);
    {
        int i = 0;
        while (i < SCREEN_WIDTH) {
            int pal = palettes[i];
            int j = i;
            while (j < SCREEN_WIDTH && palettes[j] == pal) j++;
            ansi_set_palette(pal);
            out_write((char *)chars + i, j - i);
            /* Update shadow for rendered range. */
            for (x = i; x < j; x++)
                shadow_update(x, screen_y, chars[x], palettes[x]);
            i = j;
        }
    }
}

/* ------------------------------------------------------------------ */
/* Header rendering                                                    */
/* ------------------------------------------------------------------ */

static int last_header_cursor_line = -1;
static int last_header_cursor_col  = -1;
static int last_header_modified    = -1;

static void render_header(editor_t *ed)
{
    char buf[SCREEN_WIDTH + 1];
    int pos = 0;
    int x;

    ansi_goto(0, HEADER_ROW);
    ansi_set_palette(PAL_HEADER);

    /* Format: " edit2 — <filename>  L:<line> C:<col> [modified] " */
    buf[pos++] = ' ';
    buf[pos++] = 'e';
    buf[pos++] = 'd';
    buf[pos++] = 'i';
    buf[pos++] = 't';
    buf[pos++] = '2';
    buf[pos++] = ' ';
    buf[pos++] = 0x96; /* em-dash approximation */
    buf[pos++] = ' ';

    {
        int fn_len = 0;
        while (ed->filename[fn_len] != 0 && pos + fn_len < SCREEN_WIDTH - 20)
            fn_len++;
        for (x = 0; x < fn_len; x++)
            buf[pos++] = ed->filename[x];
    }

    /* Pad with spaces until column ~25 */
    while (pos < 25) { buf[pos++] = ' '; }

    /* L:<line> */
    buf[pos++] = 'L';
    buf[pos++] = ':';
    ansi_emit_uint(ed->cursor_line + 1, buf, &pos);
    buf[pos++] = ' ';

    /* C:<col> */
    buf[pos++] = 'C';
    buf[pos++] = ':';
    ansi_emit_uint(ed->cursor_col + 1, buf, &pos);

    /* [modified] indicator */
    if (ed->modified) {
        buf[pos++] = ' ';
        buf[pos++] = '[';
        buf[pos++] = 'm';
        buf[pos++] = 'o';
        buf[pos++] = 'd';
        buf[pos++] = 'i';
        buf[pos++] = 'f';
        buf[pos++] = 'i';
        buf[pos++] = 'e';
        buf[pos++] = 'd';
        buf[pos++] = ']';
    }

    /* Pad to SCREEN_WIDTH with spaces. */
    while (pos < SCREEN_WIDTH) buf[pos++] = ' ';

    out_write(buf, SCREEN_WIDTH);
}

static void render_header_conditional(editor_t *ed)
{
    if (ed->cursor_line == last_header_cursor_line &&
        ed->cursor_col == last_header_cursor_col &&
        ed->modified == last_header_modified)
        return;
    render_header(ed);
    last_header_cursor_line = ed->cursor_line;
    last_header_cursor_col  = ed->cursor_col;
    last_header_modified    = ed->modified;
}

/* ------------------------------------------------------------------ */
/* Status bar rendering                                                */
/* ------------------------------------------------------------------ */

static void render_status(editor_t *ed)
{
    char buf[SCREEN_WIDTH + 1];
    int pos = 0;
    int x;

    ansi_goto(0, STATUS_ROW);
    ansi_set_palette(PAL_STATUS);

    /* " BDOS edit2 — <total_lines> lines  Ctrl+S:Save  Esc:Quit " */
    buf[pos++] = ' ';
    buf[pos++] = 'B';
    buf[pos++] = 'D';
    buf[pos++] = 'O';
    buf[pos++] = 'S';
    buf[pos++] = ' ';
    buf[pos++] = 'e';
    buf[pos++] = 'd';
    buf[pos++] = 'i';
    buf[pos++] = 't';
    buf[pos++] = '2';
    buf[pos++] = ' ';
    buf[pos++] = 0x96;
    buf[pos++] = ' ';

    {
        int total = lt_line_count(ed->lt);
        ansi_emit_uint(total, buf, &pos);
    }

    buf[pos++] = ' ';
    buf[pos++] = 'l';
    buf[pos++] = 'i';
    buf[pos++] = 'n';
    buf[pos++] = 'e';
    buf[pos++] = 's';

    /* Pad to SCREEN_WIDTH and append help text. */
    while (pos < SCREEN_WIDTH - 20) buf[pos++] = ' ';

    buf[pos++] = 'C';
    buf[pos++] = 't';
    buf[pos++] = 'r';
    buf[pos++] = 'l';
    buf[pos++] = '+';
    buf[pos++] = 'S';
    buf[pos++] = ':';
    buf[pos++] = 'S';
    buf[pos++] = 'a';
    buf[pos++] = 'v';
    buf[pos++] = 'e';
    buf[pos++] = ' ';
    buf[pos++] = 'E';
    buf[pos++] = 's';
    buf[pos++] = 'c';
    buf[pos++] = ':';
    buf[pos++] = 'Q';
    buf[pos++] = 'u';
    buf[pos++] = 'i';
    buf[pos++] = 't';

    /* Pad remainder with spaces. */
    while (pos < SCREEN_WIDTH) buf[pos++] = ' ';

    out_write(buf, SCREEN_WIDTH);
    status_bar_rendered = 1;
}

static void render_status_conditional(editor_t *ed)
{
    if (!status_bar_rendered) {
        render_status(ed);
    }
}

/* ------------------------------------------------------------------ */
/* Save status display                                                 */
/* ------------------------------------------------------------------ */

void render_show_save_status(int success)
{
    char buf[SCREEN_WIDTH + 1];
    int pos = 0;
    int x;

    ansi_goto(0, STATUS_ROW);
    ansi_set_palette(PAL_STATUS);

    if (success) {
        buf[pos++] = ' ';
        buf[pos++] = 'S';
        buf[pos++] = 'a';
        buf[pos++] = 'v';
        buf[pos++] = 'e';
        buf[pos++] = 'd';
        buf[pos++] = '!';
    } else {
        buf[pos++] = ' ';
        buf[pos++] = 'S';
        buf[pos++] = 'a';
        buf[pos++] = 'v';
        buf[pos++] = 'e';
        buf[pos++] = ' ';
        buf[pos++] = 'E';
        buf[pos++] = 'R';
        buf[pos++] = 'R';
    }

    while (pos < SCREEN_WIDTH) buf[pos++] = ' ';

    out_write(buf, SCREEN_WIDTH);
    out_flush();
    status_bar_rendered = 0;
}

void render_status_dirty(void)
{
    /* Mark the status bar as needing re-render on next cycle. */
    status_bar_rendered = 0;
}

/* ------------------------------------------------------------------ */
/* Confirm prompt                                                      */
/* ------------------------------------------------------------------ */

int render_confirm(char *prompt)
{
    char buf[SCREEN_WIDTH + 1];
    int pos = 0;
    char ch;

    ansi_goto(0, STATUS_ROW);
    ansi_set_palette(PAL_STATUS);

    /* Write prompt. */
    {
        int plen = 0;
        while (prompt[plen] != 0 && plen < SCREEN_WIDTH) plen++;
        for (pos = 0; pos < plen; pos++) buf[pos] = prompt[pos];
    }
    while (pos < SCREEN_WIDTH) buf[pos++] = ' ';

    out_write(buf, SCREEN_WIDTH);
    out_flush();

    /* Wait for y/n. */
    do {
        sys_read(0, &ch, 1);
    } while (ch != 'y' && ch != 'Y' && ch != 'n' && ch != 'N');

    return (ch == 'y' || ch == 'Y') ? 1 : 0;
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

static editor_t *g_editor = NULL;

void render_init(void)
{
    int y, x;
    for (y = 0; y < SCREEN_HEIGHT; y++)
        for (x = 0; x < SCREEN_WIDTH; x++) {
            shadow[y][x].tile = 0;
            shadow[y][x].palette = 0;
        }
    shadow_initialized = 0;
    status_bar_rendered = 0;
    last_header_cursor_line = -1;
    last_header_cursor_col  = -1;
    last_header_modified    = -1;
}

void render_refresh(void)
{
    if (g_editor != NULL) {
        shadow_initialized = 0;
        status_bar_rendered = 0;
        last_header_cursor_line = -1;
        last_header_cursor_col  = -1;
        last_header_modified    = -1;
        render_all(g_editor);
    }
}

void render_all(editor_t *ed)
{
    int old_scroll_y = ed->scroll_y;
    int old_scroll_x = ed->scroll_x;
    int dy, dx;
    int sy;

    g_editor = ed;

    /* Ensure cursor is visible (updates scroll_y/x). */
    ensure_cursor_visible(ed);

    dy = ed->scroll_y - old_scroll_y;
    dx = ed->scroll_x - old_scroll_x;

    if (dy != 0) {
        if (dy > 0) shadow_scroll_down(dy);
        else         shadow_scroll_up(-dy);
    } else if (dx != 0) {
        shadow_invalidate_text_area();
    }

    render_header_conditional(ed);

    /* Render text rows. */
    for (sy = 1; sy <= TEXT_ROWS; sy++) {
        int doc_line = ed->scroll_y + sy - 1;
        int total = lt_line_count(ed->lt);
        if (doc_line >= total) {
            /* Empty row below document — clear if not already blank. */
            if (shadow[sy][0].tile != 0) {
                ansi_goto(0, sy);
                ansi_set_palette(PAL_DEFAULT);
                {
                    int x;
                    for (x = 0; x < SCREEN_WIDTH; x++) {
                        out_write(" ", 1);
                        shadow_update(x, sy, ' ', PAL_DEFAULT);
                    }
                }
            }
        } else {
            render_text_row(ed, sy, doc_line);
        }
    }

    render_status_conditional(ed);
    out_flush();
}
