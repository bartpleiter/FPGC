/*
 * shell_exec.c \u2014 BDOS shell executor.
 *
 * Walks a parsed sh_chain_t. For each pipeline:
 *   1. Open redirect files via the VFS, capturing fds.
 *   2. For each command in the pipeline:
 *        a. dup2 the appropriate stdin/stdout into fds 0/1.
 *        b. If the command is a built-in, call it directly (mutating
 *           the shell's own fds while the dup2 is in effect).
 *        c. Otherwise resolve via $PATH and call bdos_proc_spawn,
 *           which inherits stdio from the shell (now redirected).
 *        d. Restore the shell's original 0/1/2 after each command.
 *   3. Pipe stages are wired through temp files in /tmp/.pipe.<seq>.
 *      Each stage runs to completion before the next starts (v2.0
 *      synchronous model).
 *   4. Honour && / || / ; chain operators between pipelines.
 *
 * Built-ins live in a static registry; they're looked up by name
 * before $PATH resolution so 'cd' can mutate the shell's cwd, etc.
 */

#include "bdos.h"

/* The shell's std fds are always 0/1/2 (BDOS pre-opens them).
 * Rather than implementing a full save/restore, after each command
 * we close-and-reopen fd 0 (RDONLY) and fd 1 (WRONLY) to /dev/tty.
 * fd 2 (stderr) is never redirected by the shell. */

static unsigned int g_pipe_seq = 0;

int          bdos_shell_last_exit = 0;
int          bdos_shell_prog_argc = 0;
char        *bdos_shell_prog_argv[BDOS_SHELL_ARGV_MAX];

/* ---- Built-in registry ---- */

typedef int (*builtin_fn_t)(int argc, char **argv);

typedef struct {
    const char  *name;
    builtin_fn_t fn;
} builtin_entry_t;

static const builtin_entry_t builtins[] = {
    { "help",    bi_help    },
    { "clear",   bi_clear   },
    { "echo",    bi_echo    },
    { "uptime",  bi_uptime  },
    { "pwd",     bi_pwd     },
    { "cd",      bi_cd      },
    { "ls",      bi_ls      },
    { "mkdir",   bi_mkdir   },
    { "mkfile",  bi_mkfile  },
    { "rm",      bi_rm      },
    { "cat",     bi_cat     },
    { "write",   bi_write   },
    { "cp",      bi_cp      },
    { "mv",      bi_mv      },
    /* `format` is now an external program: /bin/format. The boot-time
       mount-failure prompt still drives the in-kernel wizard via
       bdos_shell_start_format_wizard(); see shell_format.c. */
    { "sync",    bi_sync    },
    { "df",      bi_df      },
    { "jobs",    bi_jobs    },
    { "kill",    bi_kill    },
    { "fg",      bi_fg      },
    { "export",  bi_export  },
    { "set",     bi_set     },
    { "unset",   bi_unset   },
    { "env",     bi_env     },
    { "exit",    bi_exit    },
    { "true",    bi_true    },
    { "false",   bi_false   },
    { NULL,      NULL       }
};

static builtin_fn_t find_builtin(const char *name)
{
    int i;
    for (i = 0; builtins[i].name; i++)
        if (strcmp(builtins[i].name, name) == 0) return builtins[i].fn;
    return NULL;
}

/* ---- Stdio reset ---- */

static void reset_to_tty(int fd)
{
    int new_fd;
    int flags = (fd == 0) ? BDOS_O_RDONLY : BDOS_O_WRONLY;
    bdos_vfs_close(fd);
    new_fd = bdos_vfs_open("/dev/tty", flags);
    if (new_fd != fd && new_fd >= 0) {
        bdos_vfs_dup2(new_fd, fd);
        bdos_vfs_close(new_fd);
    }
}

static void reset_stdio(void)
{
    reset_to_tty(0);
    reset_to_tty(1);
}

/* ---- Open helpers (resolve relative paths against cwd) ---- */

static int resolve_and_open(const char *path, int flags, char *resolved)
{
    int rc = bdos_shell_resolve_path((char *)path, resolved);
    if (rc != BRFS_OK) {
        bdos_shell_print_fs_error("open", rc);
        return -1;
    }
    return bdos_vfs_open(resolved, flags);
}

/* Apply per-command redirects (< > >>) on top of whatever stdio is
 * currently in place. Returns 0 on success, -1 on error. */
static int apply_redirects(const sh_cmd_t *cmd)
{
    char path[BDOS_SHELL_PATH_MAX];
    int  fd;

    if (cmd->redir_in) {
        fd = resolve_and_open(cmd->redir_in, BDOS_O_RDONLY, path);
        if (fd < 0) {
            term2_puts("shell: cannot open input: ");
            term2_puts(cmd->redir_in);
            term2_putchar('\n');
            return -1;
        }
        bdos_vfs_dup2(fd, 0);
        bdos_vfs_close(fd);
    }
    if (cmd->redir_out) {
        fd = resolve_and_open(cmd->redir_out,
                              BDOS_O_WRONLY | BDOS_O_CREAT | BDOS_O_TRUNC, path);
        if (fd < 0) {
            term2_puts("shell: cannot open output: ");
            term2_puts(cmd->redir_out);
            term2_putchar('\n');
            return -1;
        }
        bdos_vfs_dup2(fd, 1);
        bdos_vfs_close(fd);
    }
    if (cmd->redir_append) {
        fd = resolve_and_open(cmd->redir_append,
                              BDOS_O_WRONLY | BDOS_O_CREAT | BDOS_O_APPEND, path);
        if (fd < 0) {
            term2_puts("shell: cannot open output: ");
            term2_puts(cmd->redir_append);
            term2_putchar('\n');
            return -1;
        }
        bdos_vfs_dup2(fd, 1);
        bdos_vfs_close(fd);
        bdos_vfs_lseek(1, 0, BDOS_SEEK_END);
    }
    return 0;
}

/* ---- Variable assignment (NAME=value) ---- */

static int try_assignment(const char *word)
{
    const char *eq;
    char        name[BDOS_SHELL_VAR_NAME];
    int         i;

    eq = strchr((char *)word, '=');
    if (!eq || eq == word) return 0;
    if ((eq - word) >= BDOS_SHELL_VAR_NAME) return 0;

    /* Verify name part is identifier-like. */
    for (i = 0; i < (eq - word); i++) {
        char c = word[i];
        int  ok = (c == '_') ||
                  (c >= 'A' && c <= 'Z') ||
                  (c >= 'a' && c <= 'z') ||
                  (i > 0 && c >= '0' && c <= '9');
        if (!ok) return 0;
        name[i] = c;
    }
    name[i] = 0;
    bdos_shell_var_set(name, eq + 1);
    return 1;
}

/* ---- Run a single command (built-in or external) ---- */

int bdos_shell_run_cmd(int argc, char **argv)
{
    builtin_fn_t bi;
    char         prog_path[BDOS_SHELL_PATH_MAX];
    int          rc;

    if (argc <= 0 || !argv[0]) return 0;

    bi = find_builtin(argv[0]);
    if (bi) return bi(argc, argv);

    if (!bdos_shell_require_fs_ready()) return 1;

    if (bdos_shell_resolve_program(argv[0], prog_path) != BRFS_OK ||
        !brfs_exists(prog_path) || brfs_is_dir(prog_path)) {
        term2_puts(argv[0]);
        term2_puts(": command not found\n");
        return 127;
    }

    /* If the file starts with "#!" treat it as a script. We peek by
     * opening the resolved path through the VFS. */
    {
        int  fd = bdos_vfs_open(prog_path, BDOS_O_RDONLY);
        char hdr[2];
        int  n;
        if (fd >= 0) {
            n = bdos_vfs_read(fd, hdr, 2);
            bdos_vfs_close(fd);
            if (n == 2 && hdr[0] == '#' && hdr[1] == '!')
                return bdos_shell_run_script(prog_path, argc, argv);
        }
    }

    rc = bdos_proc_spawn(prog_path, argc, argv);
    return rc;
}

/* ---- Pipeline runner ---- */

static int run_pipeline(const sh_pipeline_t *pl)
{
    char        pipe_a[BDOS_SHELL_PATH_MAX];
    char        pipe_b[BDOS_SHELL_PATH_MAX];
    const char *prev_pipe = NULL;
    int         i;
    int         exit_code = 0;

    (void)pipe_a; /* used through alias below */

    for (i = 0; i < pl->n_cmds; i++) {
        const sh_cmd_t *cmd       = &pl->cmds[i];
        const char     *next_pipe = NULL;
        int             redir_ok;

        /* Allocate a temp file for the boundary between this stage
         * and the next, if any. */
        if (i < pl->n_cmds - 1) {
            int n;
            char *p = pipe_b;
            const char *pre = "/tmp/.pipe.";
            for (n = 0; pre[n]; n++) *p++ = pre[n];
            bdos_shell_u32_to_str(g_pipe_seq++, p);
            next_pipe = pipe_b;
            brfs_create_file(next_pipe);
        }

        /* Wire stdin from the previous pipe stage's temp file. */
        if (prev_pipe) {
            int fd = bdos_vfs_open(prev_pipe, BDOS_O_RDONLY);
            if (fd >= 0) { bdos_vfs_dup2(fd, 0); bdos_vfs_close(fd); }
        }
        /* Wire stdout to the next pipe stage's temp file. */
        if (next_pipe) {
            int fd = bdos_vfs_open(next_pipe,
                                   BDOS_O_WRONLY | BDOS_O_CREAT | BDOS_O_TRUNC);
            if (fd >= 0) { bdos_vfs_dup2(fd, 1); bdos_vfs_close(fd); }
        }

        /* Apply per-command < > >> redirects on top of pipe wiring. */
        redir_ok = apply_redirects(cmd);
        if (redir_ok < 0) {
            exit_code = 1;
        } else {
            /* Bare `name=value` with no command — treat as assignment. */
            if (cmd->argc == 1 && try_assignment(cmd->argv[0])) {
                exit_code = 0;
            } else {
                exit_code = bdos_shell_run_cmd(cmd->argc, (char **)cmd->argv);
            }
        }

        reset_stdio();

        /* Delete the just-consumed prev_pipe temp file. */
        if (prev_pipe) brfs_delete(prev_pipe);
        if (next_pipe) {
            /* Move next_pipe \u2192 pipe_a so we can reuse pipe_b next iter. */
            int n;
            for (n = 0; next_pipe[n]; n++) pipe_a[n] = next_pipe[n];
            pipe_a[n] = 0;
            prev_pipe = pipe_a;
        } else {
            prev_pipe = NULL;
        }
    }

    if (prev_pipe) brfs_delete(prev_pipe);
    return exit_code;
}

/* ---- Chain runner ---- */

int bdos_shell_run_chain(sh_chain_t *chain)
{
    int i;
    int last_exit = 0;
    int skip_until_semi = 0;

    for (i = 0; i < chain->n_pipes; i++) {
        if (!skip_until_semi) {
            last_exit = run_pipeline(&chain->pipes[i]);
            bdos_shell_last_exit = last_exit;
        }
        switch (chain->ops[i]) {
            case SH_OP_AND:
                if (last_exit != 0) skip_until_semi = 1;
                break;
            case SH_OP_OR:
                if (last_exit == 0) skip_until_semi = 1;
                break;
            case SH_OP_SEMI:
            case SH_OP_END:
                skip_until_semi = 0;
                break;
        }
    }
    return last_exit;
}

/* ---- Top-level entry point ---- */

int bdos_shell_execute_line(char *line)
{
    char       expanded[BDOS_SHELL_INPUT_MAX * 2];
    char       store   [BDOS_SHELL_INPUT_MAX * 2];
    sh_tok_t   toks[BDOS_SHELL_TOK_MAX];
    sh_chain_t chain;
    int        n;

    if (bdos_shell_expand(line, expanded, sizeof(expanded), 0, NULL) < 0) {
        term2_puts("shell: bad quoting / overflow\n");
        bdos_shell_last_exit = 1;
        return 1;
    }

    n = bdos_shell_lex(expanded, toks, BDOS_SHELL_TOK_MAX,
                       store, sizeof(store));
    if (n < 0) {
        term2_puts("shell: lex error\n");
        bdos_shell_last_exit = 1;
        return 1;
    }
    if (n == 0) return bdos_shell_last_exit;

    if (bdos_shell_parse(toks, &chain) < 0) {
        bdos_shell_last_exit = 1;
        return 1;
    }
    return bdos_shell_run_chain(&chain);
}
