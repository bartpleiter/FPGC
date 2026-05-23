#include <string.h>
#include <syscall.h>
#include "render.h"
#include "editor.h"
#include "gapbuf.h"
#include "line_table.h"
#include "input.h"

/* Palette indices */
#define PAL_DEFAULT    0   /* White on black */
#define PAL_HEADER     1   /* Black on white (inverted) */
#define PAL_STATUS     28  /* Black on yellow */
#define PAL_CURSOR     1   /* Black on white (inverted) */

/* Output buffer */
#define OUT_BUF_SIZE 8192
static char out_buf[OUT_BUF_SIZE];
static int  out_buf_n = 0;

void out_flush(void)
{
    if (out_buf_n > 0) {
        sys_write(1, out_buf, out_buf_n);
        out_buf_n = 0;
    }
}

void out_write(const char *s, int n)
{
    int i;
    if (n <= 0) return;
    if (n >= OUT_BUF_SIZE) { out_flush(); sys_write(1, (char *)s, n); return; }
    if (out_buf_n + n > OUT_BUF_SIZE) out_flush();
    for (i = 0; i < n; i++) out_buf[out_buf_n + i] = s[i];
    out_buf_n += n;
}

static int ansi_strlen(const char *s)
{
    int n = 0;
    while (s[n]) n++;
    return n;
}

void ansi_write(const char *s)
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

static int last_palette = -1;

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

/* Shadow buffer */
struct shadow_cell {
    unsigned char tile;
    unsigned char palette;
};

static struct shadow_cell shadow[SCREEN_HEIGHT][SCREEN_WIDTH];
static int shadow_initialized = 0;

/* Header/status tracking */
static int last_header_cursor_line = -1;
static int last_header_cursor_col = -1;
static int last_header_modified = -1;
static int status_bar_rendered = 0;

/* Previous scroll state */
static int prev_scroll_y = 0;
static int prev_scroll_x = 0;

void render_init(void)
{
    memset(shadow, 0, sizeof(shadow));
    shadow_initialized = 1;
    last_palette = -1;
    last_header_cursor_line = -1;
    last_header_cursor_col = -1;
    last_header_modified = -1;
    status_bar_rendered = 0;
    prev_scroll_y = 0;
    prev_scroll_x = 0;
}

static void shadow_invalidate_row(int y)
{
    memset(shadow[y], 0, sizeof(shadow[y]));
}

static void shadow_invalidate_text_area(void)
{
    int y;
    for (y = 1; y <= TEXT_ROWS; y++)
        shadow_invalidate_row(y);
}

/* Render a single row from char/palette arrays using batch technique */
static void render_row(int screen_y, unsigned char *chars, unsigned char *pals)
{
    int run_start, run_end;
    int changed, x;

    /* Check if entire row matches shadow */
    if (shadow_initialized) {
        changed = 0;
        for (x = 0; x < SCREEN_WIDTH; x++) {
            if (shadow[screen_y][x].tile != chars[x] ||
                shadow[screen_y][x].palette != pals[x]) {
                changed = 1;
                break;
            }
        }
        if (!changed) return; /* Row unchanged */
    }

    /* Emit row with palette grouping */
    ansi_goto(0, screen_y);
    run_start = 0;
    while (run_start < SCREEN_WIDTH) {
        run_end = run_start;
        while (run_end < SCREEN_WIDTH && pals[run_end] == pals[run_start])
            run_end++;
        ansi_set_palette(pals[run_start]);
        out_write((char *)&chars[run_start], run_end - run_start);
        run_start = run_end;
    }

    /* Update shadow */
    for (x = 0; x < SCREEN_WIDTH; x++) {
        shadow[screen_y][x].tile = chars[x];
        shadow[screen_y][x].palette = pals[x];
    }
}

static void render_header(editor_t *ed)
{
    unsigned char chars[SCREEN_WIDTH];
    unsigned char pals[SCREEN_WIDTH];
    int i, c;
    char pos_buf[20];
    int pos_len, pos_start;

    memset(chars, ' ', SCREEN_WIDTH);
    memset(pals, PAL_HEADER, SCREEN_WIDTH);

    /* Filename */
    c = 0;
    for (i = 0; ed->filename[i] && i < 18; i++) {
        chars[i] = ed->filename[i];
        c++;
    }
    /* Modified indicator */
    if (ed->modified) {
        if (c < SCREEN_WIDTH - 1) {
            chars[c] = '*';
        }
    }

    /* Line:Col position (right-aligned) */
    {
        int line_num = ed->cursor_line + 1;
        int col_num = ed->cursor_col + 1;
        int n = 0;
        pos_buf[n++] = ' ';
        /* Format line number */
        {
            char tmp[12];
            int tn = 0;
            int v = line_num;
            if (v == 0) { tmp[tn++] = '0'; }
            else { while (v > 0) { tmp[tn++] = '0' + (v % 10); v /= 10; } }
            while (tn > 0) pos_buf[n++] = tmp[--tn];
        }
        pos_buf[n++] = ':';
        /* Format col number */
        {
            char tmp[12];
            int tn = 0;
            int v = col_num;
            if (v == 0) { tmp[tn++] = '0'; }
            else { while (v > 0) { tmp[tn++] = '0' + (v % 10); v /= 10; } }
            while (tn > 0) pos_buf[n++] = tmp[--tn];
        }
        pos_len = n;
        pos_start = SCREEN_WIDTH - pos_len;
        if (pos_start < 0) pos_start = 0;
        for (i = 0; i < pos_len && pos_start + i < SCREEN_WIDTH; i++)
            chars[pos_start + i] = pos_buf[i];
    }

    render_row(HEADER_ROW, chars, pals);
    last_header_cursor_line = ed->cursor_line;
    last_header_cursor_col = ed->cursor_col;
    last_header_modified = ed->modified;
}

static void render_header_conditional(editor_t *ed)
{
    if (ed->cursor_line == last_header_cursor_line &&
        ed->cursor_col == last_header_cursor_col &&
        ed->modified == last_header_modified)
        return;
    render_header(ed);
}

static void render_status(void)
{
    unsigned char chars[SCREEN_WIDTH];
    unsigned char pals[SCREEN_WIDTH];
    int i;

    memset(chars, ' ', SCREEN_WIDTH);
    memset(pals, PAL_STATUS, SCREEN_WIDTH);

    /* Static status bar text */
    {
        const char *msg = " ESC:quit ^S:save ^L:refresh";
        for (i = 0; msg[i] && i < SCREEN_WIDTH; i++)
            chars[i] = msg[i];
    }

    render_row(STATUS_ROW, chars, pals);
    status_bar_rendered = 1;
}

static void render_status_conditional(void)
{
    if (!status_bar_rendered)
        render_status();
}

static void render_text(editor_t *ed)
{
    int screen_row, doc_line, col, doc_col;
    int line_len, line_off;
    int total_lines;
    unsigned char chars[SCREEN_WIDTH];
    unsigned char pals[SCREEN_WIDTH];
    int cursor_screen_y, cursor_screen_x;

    total_lines = lt_line_count(ed->lt);
    cursor_screen_y = 1 + (ed->cursor_line - ed->scroll_y);
    cursor_screen_x = ed->cursor_col - ed->scroll_x;

    for (screen_row = 0; screen_row < TEXT_ROWS; screen_row++) {
        doc_line = ed->scroll_y + screen_row;

        memset(chars, ' ', SCREEN_WIDTH);
        memset(pals, PAL_DEFAULT, SCREEN_WIDTH);

        if (doc_line < total_lines) {
            line_off = lt_line_start(ed->lt, doc_line);
            line_len = lt_line_length(ed->lt, doc_line, gapbuf_len(ed->gb));

            for (col = 0; col < SCREEN_WIDTH; col++) {
                doc_col = ed->scroll_x + col;
                if (doc_col < line_len) {
                    unsigned char ch;
                    ch = gapbuf_at(ed->gb, line_off + doc_col);
                    if (ch < 0x20 || ch == 0x7F) ch = ' ';
                    chars[col] = ch;
                }
            }
        }

        /* Apply cursor highlight */
        if (screen_row + 1 == cursor_screen_y &&
            cursor_screen_x >= 0 && cursor_screen_x < SCREEN_WIDTH) {
            pals[cursor_screen_x] = PAL_CURSOR;
        }

        render_row(screen_row + 1, chars, pals);
    }
}

static void ensure_cursor_visible(editor_t *ed)
{
    if (ed->cursor_line < ed->scroll_y)
        ed->scroll_y = ed->cursor_line;
    if (ed->cursor_line >= ed->scroll_y + TEXT_ROWS)
        ed->scroll_y = ed->cursor_line - TEXT_ROWS + 1;
    if (ed->cursor_col < ed->scroll_x)
        ed->scroll_x = ed->cursor_col;
    if (ed->cursor_col >= ed->scroll_x + SCREEN_WIDTH)
        ed->scroll_x = ed->cursor_col - SCREEN_WIDTH + 1;
}

void render_all(editor_t *ed)
{
    int old_scroll_y, old_scroll_x;
    int dy, dx;

    old_scroll_y = ed->scroll_y;
    old_scroll_x = ed->scroll_x;

    ensure_cursor_visible(ed);

    dy = ed->scroll_y - old_scroll_y;
    dx = ed->scroll_x - old_scroll_x;

    if (dy != 0 || dx != 0) {
        /* For simplicity in v1, invalidate all text rows on any scroll change.
           Shadow shift optimization can be added later. */
        shadow_invalidate_text_area();
    }

    render_header_conditional(ed);
    render_text(ed);
    render_status_conditional();

    /* Position cursor last (§14.5) */
    {
        int cx, cy;
        cx = ed->cursor_col - ed->scroll_x;
        cy = 1 + (ed->cursor_line - ed->scroll_y);
        if (cx >= 0 && cx < SCREEN_WIDTH && cy >= 1 && cy <= TEXT_ROWS)
            ansi_goto(cx, cy);
    }

    out_flush();

    prev_scroll_y = ed->scroll_y;
    prev_scroll_x = ed->scroll_x;
}

void render_refresh(void)
{
    memset(shadow, 0, sizeof(shadow));
    last_palette = -1;
    last_header_cursor_line = -1;
    last_header_cursor_col = -1;
    last_header_modified = -1;
    status_bar_rendered = 0;
}

void render_status_dirty(void)
{
    status_bar_rendered = 0;
}

void render_show_save_status(int success)
{
    unsigned char chars[SCREEN_WIDTH];
    unsigned char pals[SCREEN_WIDTH];
    int i;
    const char *msg;

    memset(chars, ' ', SCREEN_WIDTH);
    memset(pals, PAL_STATUS, SCREEN_WIDTH);

    msg = success ? " Saved!" : " Save ERR";
    for (i = 0; msg[i] && i < SCREEN_WIDTH; i++)
        chars[i] = msg[i];

    render_row(STATUS_ROW, chars, pals);
    out_flush();
    status_bar_rendered = 0;
}

void render_show_prompt(const char *prompt)
{
    unsigned char chars[SCREEN_WIDTH];
    unsigned char pals[SCREEN_WIDTH];
    int i;

    memset(chars, ' ', SCREEN_WIDTH);
    memset(pals, PAL_STATUS, SCREEN_WIDTH);

    for (i = 0; prompt[i] && i < SCREEN_WIDTH; i++)
        chars[i] = prompt[i];

    render_row(STATUS_ROW, chars, pals);
    out_flush();
    status_bar_rendered = 0;
}

int render_confirm(const char *prompt, editor_t *ed)
{
    unsigned char chars[SCREEN_WIDTH];
    unsigned char pals[SCREEN_WIDTH];
    int i, key;

    memset(chars, ' ', SCREEN_WIDTH);
    memset(pals, PAL_STATUS, SCREEN_WIDTH);

    for (i = 0; prompt[i] && i < SCREEN_WIDTH; i++)
        chars[i] = prompt[i];

    render_row(STATUS_ROW, chars, pals);
    out_flush();

    key = input_read_key();
    status_bar_rendered = 0;
    return (key == 'y' || key == 'Y');
}
