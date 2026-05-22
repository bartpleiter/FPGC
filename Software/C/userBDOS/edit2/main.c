#include <syscall.h>
#include "editor.h"
#include "render.h"
#include "fileio.h"
#include "input.h"

#define SCREEN_WIDTH   40
#define SCREEN_HEIGHT  25
#define TEXT_ROWS      23
#define CTRL_S         19
#define CTRL_L         12
#define KEY_ESCAPE     27

typedef void (*key_handler_t)(editor_t *ed);

struct key_binding {
    int key;
    key_handler_t handler;
};

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
    { 0x08,         editor_backspace },    // Backspace (Ctrl+H)
    { 127,          editor_backspace },    // Delete key (DEL)
    { '\n',         editor_insert_newline },
    { '\t',         editor_insert_tab },
};

static const int binding_count = sizeof(bindings) / sizeof(bindings[0]);

static int running = 1;

static void handle_save(editor_t *ed);
static void handle_quit(editor_t *ed);

static void handle_save(editor_t *ed) {
    const char *path = ed->filepath[0] ? ed->filepath : 0;
    int rc;
    if (path) {
        rc = file_save(ed, path);
    } else {
        /* No file path — can't save without a name */
        rc = -1;
    }
    render_show_save_status(rc == 0);
}

static void handle_quit(editor_t *ed) {
    if (editor_is_modified(ed)) {
        int confirmed = render_confirm("Save changes? (y/n)");
        if (confirmed) {
            handle_save(ed);
        }
    }
    running = 0;
}

int main(void) {
    /* 1. Parse arguments — get filename from argv */
    const char *filepath = 0;
    int ac = sys_argc();
    if (ac > 1) {
        char **av = sys_argv();
        filepath = av[1];
    }

    /* 2. Create gap buffer with initial size */
    gapbuf_t *gb = gapbuf_create(8192);

    /* 3. Create line table */
    line_table_t *lt = line_table_create();

    /* 4. Create editor state */
    editor_t *ed = editor_create(gb, lt);

    /* 5. Load file if specified */
    if (filepath) {
        if (file_load(ed, filepath) != 0) {
            /* File not found — start with empty buffer */
        }
    } else {
        ed->filepath[0] = 0;
        ed->filename[0] = 0;
    }

    /* 6. Enter alternate screen */
    sys_write(1, "\x1b[?1049h", 8);

    /* 7. Initialize input and render */
    input_init();
    render_init();

    /* 8. Initial full render */
    render_all(ed);

    /* 9. Event loop */
    while (running) {
        int key = input_read_key();

        /* Lookup in binding table */
        int handled = 0;
        for (int i = 0; i < binding_count; i++) {
            if (key == bindings[i].key) {
                bindings[i].handler(ed);
                handled = 1;
                break;
            }
        }

        /* Special keys not in table */
        if (!handled) {
            if (key == CTRL_S) {
                handle_save(ed);
                handled = 1;
            } else if (key == KEY_ESCAPE) {
                handle_quit(ed);
                handled = 1;
            } else if (key == CTRL_L) {
                render_refresh();
                handled = 1;
            }
        }

        /* Printable ASCII -> insert character */
        if (!handled && key >= 32 && key <= 126) {
            editor_insert_char(ed, (unsigned char)key);
        }
        /* Otherwise: silently ignore (control character filtering) */

        /* Render after each action */
        render_all(ed);
    }

    /* 10. Cleanup */
    sys_write(1, "\x1b[?1049l", 8);  // Leave alternate screen
    input_close();
    editor_destroy(ed);
    line_table_destroy(lt);
    gapbuf_destroy(gb);

    sys_exit(0);
    return 0;  /* unreachable */
}
