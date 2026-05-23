#include <syscall.h>
#include "editor.h"
#include "gapbuf.h"
#include "line_table.h"
#include "render.h"
#include "input.h"
#include "fileio.h"

/* Forward declarations for ansi_write and out_flush (defined in render.c) */
void ansi_write(const char *s);
void out_flush(void);

/* Control keys */
#define CTRL_L  12
#define CTRL_S  19

static int running = 1;

typedef void (*key_handler_t)(editor_t *ed);

struct key_binding {
    int key;
    key_handler_t handler;
};

static void handle_save(editor_t *ed)
{
    int rc;
    rc = file_save(ed);
    render_show_save_status(rc == 0);
}

static void handle_quit(editor_t *ed)
{
    if (ed->modified) {
        int key;
        render_show_prompt(" Unsaved changes. Quit? (y/n)");
        /* Loop until we get a definitive y or n, ignoring ESC repeats */
        for (;;) {
            key = input_read_key();
            if (key == 'y' || key == 'Y') {
                running = 0;
                return;
            }
            if (key == 'n' || key == 'N')
                return;
            /* Ignore any other key (including repeated ESC) */
        }
    }
    running = 0;
}

static void handle_refresh(editor_t *ed)
{
    render_refresh();
    ansi_write("\x1b[2J");
}

static const struct key_binding bindings[] = {
    { KEY_LEFT,     editor_move_left },
    { KEY_RIGHT,    editor_move_right },
    { KEY_UP,       editor_move_up },
    { KEY_DOWN,     editor_move_down },
    { KEY_HOME,     editor_move_home },
    { KEY_END,      editor_move_end },
    { KEY_PAGEUP,   editor_page_up },
    { KEY_PAGEDOWN, editor_page_down },
    { KEY_DELETE,   editor_delete },
    { 0x08,         editor_backspace },
    { 127,          editor_backspace },
    { '\n',         (key_handler_t)editor_insert_newline },
    { '\t',         (key_handler_t)editor_insert_tab },
    { CTRL_S,       handle_save },
    { 27,           handle_quit },
    { CTRL_L,       handle_refresh },
};

#define NUM_BINDINGS (int)(sizeof(bindings) / sizeof(bindings[0]))

static void event_loop(editor_t *ed)
{
    int key, i, handled;

    render_all(ed);

    while (running) {
        key = input_read_key();
        if (key < 0) continue;

        handled = 0;
        for (i = 0; i < NUM_BINDINGS; i++) {
            if (bindings[i].key == key) {
                bindings[i].handler(ed);
                handled = 1;
                break;
            }
        }

        if (!handled && key >= 32 && key <= 126) {
            editor_insert_char(ed, (unsigned char)key);
        }

        if (running)
            render_all(ed);
    }
}

int main(void)
{
    gapbuf_t *gb;
    line_table_t *lt;
    editor_t *ed;
    const char *path;
    int argc;
    char **argv;

    argc = sys_argc();
    argv = sys_argv();

    if (argc < 2) {
        sys_putstr("usage: edit <file>\n");
        return 1;
    }
    path = argv[1];

    /* Create modules */
    lt = line_table_create();
    if (!lt) {
        sys_putstr("edit: out of memory (line table)\n");
        return 1;
    }

    gb = gapbuf_create(4096);
    if (!gb) {
        sys_putstr("edit: out of memory (gap buffer)\n");
        line_table_destroy(lt);
        return 1;
    }

    ed = editor_create(gb, lt);
    if (!ed) {
        sys_putstr("edit: out of memory (editor)\n");
        gapbuf_destroy(gb);
        line_table_destroy(lt);
        return 1;
    }

    /* Load file */
    if (file_load(ed, path) != 0) {
        sys_putstr("edit: failed to load file\n");
        editor_destroy(ed);
        gapbuf_destroy(gb);
        line_table_destroy(lt);
        return 1;
    }

    /* Open raw TTY for keyboard input */
    if (input_init() < 0) {
        sys_putstr("edit: cannot open /dev/tty\n");
        editor_destroy(ed);
        gapbuf_destroy(gb);
        line_table_destroy(lt);
        return 1;
    }

    /* Enter alternate screen (§14.2, §14.3, §14.7) */
    ansi_write("\x1b[?1049h");    /* Enter alternate screen */
    ansi_write("\x1b[?7l");       /* Disable DECAWM */
    ansi_write("\x1b[2;24r");     /* Scroll region = rows 2–24 (text area) */
    ansi_write("\x1b[2J\x1b[H");  /* Clear screen, home cursor */
    out_flush();

    render_init();

    /* Run editor */
    event_loop(ed);

    /* Shutdown (§14.7) */
    ansi_write("\x1b[0m");        /* Reset SGR */
    ansi_write("\x1b[r");         /* Reset scroll region */
    ansi_write("\x1b[?7h");       /* Re-enable DECAWM */
    ansi_write("\x1b[?1049l");    /* Leave alternate screen */
    out_flush();

    input_close();
    editor_destroy(ed);
    gapbuf_destroy(gb);
    line_table_destroy(lt);
    return 0;
}
