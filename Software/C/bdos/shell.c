/*
 * shell.c \u2014 BDOS interactive shell front-end.
 *
 * Owns the cwd, the input-line buffer, command history (8 entries),
 * and the per-keypress UI loop. Parsing/execution is delegated to
 * shell_exec.c. Visual behaviour (welcome banner, "cwd> " prompt,
 * software cursor, line wrap, history nav, F1-F8 resume,
 * Ctrl+C / Ctrl+L / Ctrl+Arrow) is preserved from v1.x.
 *
 * The cursor is drawn manually by inverting one cell because the
 * underlying terminal has no hardware cursor in alt-screen mode.
 */

#include "bdos.h"

/* ---- State ---- */

char         bdos_shell_cwd[BDOS_SHELL_PATH_MAX] = "/";
unsigned int bdos_shell_start_micros;

/* Line editor state \u2014 kept in one struct to make the lifecycle obvious. */
typedef struct {
    char         buf[BDOS_SHELL_INPUT_MAX];
    int          len;
    int          cursor;
    int          overflow;

    /* Saved render extent so we can erase the previous line cheaply. */
    unsigned int prompt_x;
    unsigned int prompt_y;
    int          last_render;

    /* Software cursor (we draw it ourselves on top of the rendered text). */
    int           visible;
    unsigned int  draw_x;
    unsigned int  draw_y;
    unsigned char saved_tile;
    unsigned char saved_palette;

    char         prompt[BDOS_SHELL_PROMPT_MAX];
} editor_t;

static editor_t E;

/* ---- History ring ---- */

#define HIST_N 8
typedef struct {
    char buf[HIST_N][BDOS_SHELL_INPUT_MAX];
    int  head;        /* next slot to write */
    int  count;       /* number of valid entries (<= HIST_N) */
    int  nav_offset;  /* -1 = editing fresh line; >=0 = browsing */
    char saved[BDOS_SHELL_INPUT_MAX];  /* current input snapshot */
} history_t;

static history_t H;

/* ---- Forward decls ---- */

static void render(void);
static void start_line_internal(void);

/* ---- Cursor (software, single-cell inverse) ---- */

static void cursor_clear(void)
{
    if (!E.visible) return;
    term_put_cell(E.draw_x, E.draw_y, E.saved_tile, E.saved_palette);
    E.visible = 0;
}

static void cursor_draw(void)
{
    unsigned int  x, y;
    unsigned char tile, pal;
    term_get_cursor(&x, &y);
    term_get_cell(x, y, &tile, &pal);
    E.draw_x = x;
    E.draw_y = y;
    E.saved_tile = tile;
    E.saved_palette = pal;
    E.visible = 1;
    term_put_cell(x, y, tile, PALETTE_BLACK_ON_WHITE);
}

/* ---- Prompt ---- */

static void build_prompt(void)
{
    E.prompt[0] = 0;
    strcat(E.prompt, bdos_shell_cwd);
    strcat(E.prompt, "> ");
}

static int prompt_len(void) { return strlen(E.prompt); }

/* If our last render exceeded the screen height, scroll the prompt up
 * so the input always remains on-screen. */
static void scroll_into_view(int rendered)
{
    int rows_used  = (E.prompt_x + rendered) / TERM_WIDTH;
    int overflow   = (int)E.prompt_y + rows_used - (TERM_HEIGHT - 1);
    if (overflow > 0) {
        int py = (int)E.prompt_y - overflow;
        if (py < 0) py = 0;
        E.prompt_y = (unsigned int)py;
    }
}

static void move_cursor_to_input(void)
{
    int  pl = prompt_len();
    int  abs;
    int  row, col;

    if (E.cursor < 0)        E.cursor = 0;
    if (E.cursor > E.len)    E.cursor = E.len;

    abs = pl + E.cursor;
    row = (int)E.prompt_y + ((int)E.prompt_x + abs) / TERM_WIDTH;
    col = ((int)E.prompt_x + abs) % TERM_WIDTH;
    if (row < 0)             row = 0;
    if (row >= TERM_HEIGHT)  row = TERM_HEIGHT - 1;
    term_set_cursor((unsigned int)col, (unsigned int)row);
}

static void render(void)
{
    int new_len;
    int i;

    cursor_clear();

    /* Erase old render. */
    term_set_palette(PALETTE_WHITE_ON_BLACK);
    term_set_cursor(E.prompt_x, E.prompt_y);
    for (i = 0; i < E.last_render; i++) term_putchar(' ');
    scroll_into_view(E.last_render);

    /* Re-emit prompt + input. */
    term_set_cursor(E.prompt_x, E.prompt_y);
    term_puts(E.prompt);
    term_write(E.buf, E.len);

    new_len = prompt_len() + E.len;
    E.last_render = new_len;
    scroll_into_view(new_len);

    move_cursor_to_input();
    cursor_draw();
}

/* ---- Editor primitives ---- */

static void editor_clear(void)
{
    E.len = 0;
    E.cursor = 0;
    E.buf[0] = 0;
    E.overflow = 0;
    H.nav_offset = -1;
    H.saved[0] = 0;
}

static void editor_set(const char *src)
{
    int n = strlen(src);
    if (n >= BDOS_SHELL_INPUT_MAX) n = BDOS_SHELL_INPUT_MAX - 1;
    memcpy(E.buf, (char *)src, n);
    E.buf[n] = 0;
    E.len = n;
    E.cursor = n;
    E.overflow = 0;
}

static void editor_insert(char c)
{
    int i;
    if (E.len >= BDOS_SHELL_INPUT_MAX - 1) { E.overflow = 1; return; }
    for (i = E.len; i >= E.cursor; i--) E.buf[i + 1] = E.buf[i];
    E.buf[E.cursor++] = c;
    E.len++;
    E.overflow = 0;
}

static void editor_backspace(void)
{
    int i;
    if (E.cursor <= 0) return;
    for (i = E.cursor - 1; i < E.len; i++) E.buf[i] = E.buf[i + 1];
    E.len--; E.cursor--;
    E.overflow = 0;
}

static void editor_delete(void)
{
    int i;
    if (E.cursor >= E.len) return;
    for (i = E.cursor; i < E.len; i++) E.buf[i] = E.buf[i + 1];
    E.len--;
    E.overflow = 0;
}

/* ---- History ---- */

static int hist_index(int off)
{
    int i = H.head - 1 - off;
    while (i < 0) i += HIST_N;
    return i % HIST_N;
}

static void hist_add(const char *line)
{
    int n = strlen(line);
    if (n == 0) return;
    if (n >= BDOS_SHELL_INPUT_MAX) n = BDOS_SHELL_INPUT_MAX - 1;
    memcpy(H.buf[H.head], (char *)line, n);
    H.buf[H.head][n] = 0;
    H.head = (H.head + 1) % HIST_N;
    if (H.count < HIST_N) H.count++;
}

static void hist_up(void)
{
    if (H.count == 0) return;
    if (H.nav_offset < 0) {
        memcpy(H.saved, E.buf, E.len);
        H.saved[E.len] = 0;
        H.nav_offset = 0;
    } else if (H.nav_offset < H.count - 1) {
        H.nav_offset++;
    }
    editor_set(H.buf[hist_index(H.nav_offset)]);
}

static void hist_down(void)
{
    if (H.nav_offset < 0) return;
    if (H.nav_offset == 0) {
        H.nav_offset = -1;
        editor_set(H.saved);
        return;
    }
    H.nav_offset--;
    editor_set(H.buf[hist_index(H.nav_offset)]);
}

/* ---- Public lifecycle ---- */

static void start_line_internal(void)
{
    build_prompt();
    term_get_cursor(&E.prompt_x, &E.prompt_y);
    E.last_render = 0;
    render();
}

void bdos_shell_start_line(void) { start_line_internal(); }

void bdos_shell_reset_and_prompt(void)
{
    editor_clear();
    start_line_internal();
}

static void print_welcome(void)
{
    term_puts(" ___ ___   ___  ___ \n");
    term_puts("| _ )   \\ / _ \\/ __|\n");
    term_puts("| _ \\ |) | (_) \\__ \\\n");
    term_puts("|___/___/ \\___/|___/v3.0\n\n");
}

/* ---- Submit ---- */

static void submit(void)
{
    cursor_clear();
    term_putchar('\n');

    if (E.overflow) {
        term_puts("error: input too long\n");
    } else {
        E.buf[E.len] = 0;
        if (!bdos_shell_handle_special_mode_line(E.buf)) {
            hist_add(E.buf);
            bdos_shell_execute_line(E.buf);
        }
    }
    editor_clear();
    start_line_internal();
}

/* ---- Key dispatch ---- */

static void handle_key(int k)
{
    if (k == '\n' || k == '\r') { submit(); return; }
    if (k == '\b' || k == 127)  { editor_backspace();  render(); return; }
    if (k == BDOS_KEY_DELETE)   { editor_delete();     render(); return; }
    if (k == BDOS_KEY_LEFT)     { if (E.cursor > 0)     E.cursor--; render(); return; }
    if (k == BDOS_KEY_RIGHT)    { if (E.cursor < E.len) E.cursor++; render(); return; }
    if (k == BDOS_KEY_UP)       { hist_up();   render(); return; }
    if (k == BDOS_KEY_DOWN)     { hist_down(); render(); return; }

    if (k == 3) {                       /* Ctrl+C */
        editor_clear();
        render();
        return;
    }
    if (k == 12) {                      /* Ctrl+L */
        term_clear();
        editor_clear();
        start_line_internal();
        return;
    }

    if (k >= BDOS_KEY_F1 && k <= BDOS_KEY_F8) {
        int slot = k - BDOS_KEY_F1;
        if (slot < MEM_SLOT_COUNT &&
            bdos_slot_status[slot] == BDOS_SLOT_STATUS_SUSPENDED) {
            cursor_clear();
            term_puts("\n[");
            term_putint(slot);
            term_puts("] resuming: ");
            term_puts(bdos_slot_name[slot]);
            term_putchar('\n');
            bdos_resume_program(slot);
            bdos_shell_reset_and_prompt();
        }
        return;
    }

    if (k >= 32 && k <= 126) { editor_insert((char)k); render(); }
}

/* ---- Init / tick ---- */

void bdos_shell_init(void)
{
    term_set_palette(PALETTE_WHITE_ON_BLACK);
    term_clear();

    /* Zero out editor + history. */
    {
        int i, j;
        E.len = E.cursor = E.overflow = 0;
        E.buf[0] = 0;
        E.last_render = 0;
        E.visible = 0;
        E.draw_x = E.draw_y = 0;
        E.saved_tile = 0;
        E.saved_palette = PALETTE_WHITE_ON_BLACK;
        H.head = H.count = 0;
        H.nav_offset = -1;
        H.saved[0] = 0;
        for (i = 0; i < HIST_N; i++)
            for (j = 0; j < BDOS_SHELL_INPUT_MAX; j++)
                H.buf[i][j] = 0;
    }

    bdos_shell_start_micros = get_micros();
    bdos_shell_vars_init();

    print_welcome();
    bdos_shell_on_startup();
    start_line_internal();
}

#define SCROLL_REPEAT_US 30000U
static unsigned int g_scroll_last_us = 0;

void bdos_shell_tick(void)
{
    int          k;
    unsigned int now;

    if (bdos_key_state_bitmap & KEYSTATE_CTRL) {
        now = get_micros();
        if ((unsigned int)(now - g_scroll_last_us) >= SCROLL_REPEAT_US) {
            if (bdos_key_state_bitmap & KEYSTATE_UP)   {
                term_scroll_view_up();   g_scroll_last_us = now;
            } else if (bdos_key_state_bitmap & KEYSTATE_DOWN) {
                term_scroll_view_down(); g_scroll_last_us = now;
            }
        }
    }

    while (bdos_keyboard_event_available()) {
        k = bdos_keyboard_event_read();
        if (k != -1) handle_key(k);
    }
}
