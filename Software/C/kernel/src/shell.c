/*
 * shell.c — BDOS v4 shell (Phase 1 minimal stub).
 *
 * This is a temporary in-kernel shell for Phase 1.
 * It will be replaced by a proper shell with tokenizer/parser
 * once VFS and process management are verified.
 *
 * For now: prompt → read line → parse simple command → execute.
 */
#include "kernel.h"

#define SHELL_INPUT_MAX 256
#define SHELL_CWD_MAX  128

static char shell_input[SHELL_INPUT_MAX];
static int  shell_input_len;
static int  shell_prompt_shown;
static char shell_cwd[SHELL_CWD_MAX];

/* Sync shell_cwd into the kernel process's proc.cwd so children inherit it */
static void shell_sync_cwd(void)
{
    struct proc *kp;
    int i;
    kp = proc_by_pid(0);
    if (!kp) return;
    for (i = 0; shell_cwd[i] && i < PROC_CWD_LEN - 1; i++)
        kp->cwd[i] = shell_cwd[i];
    kp->cwd[i] = '\0';
}

/* Output redirection: -1 = terminal, >= 0 = gfd for pipe/redirect */
static int shell_out_gfd = -1;

static void shell_puts(const char *s)
{
    if (shell_out_gfd >= 0)
    {
        int len;
        const char *p;
        len = 0;
        p = s;
        while (*p) { len++; p++; }
        vfs_write(shell_out_gfd, s, len);
    }
    else
    {
        term_puts(s);
    }
}

static void shell_putchar(int ch)
{
    if (shell_out_gfd >= 0)
    {
        char c;
        c = (char)ch;
        vfs_write(shell_out_gfd, &c, 1);
    }
    else
    {
        term_putchar(ch);
    }
}

/* Resolve a relative path against cwd into buf. Returns buf. */
static char *shell_resolve_path(char *buf, int bufsize, const char *path)
{
    int i;
    int len;

    if (path[0] == '/')
    {
        /* Absolute path — copy directly */
        for (i = 0; path[i] && i < bufsize - 1; i++)
            buf[i] = path[i];
        buf[i] = '\0';
        return buf;
    }

    /* Relative path — prepend cwd */
    len = 0;
    for (i = 0; shell_cwd[i] && len < bufsize - 1; i++)
        buf[len++] = shell_cwd[i];
    if (len > 1 && buf[len - 1] != '/' && len < bufsize - 1)
        buf[len++] = '/';
    for (i = 0; path[i] && len < bufsize - 1; i++)
        buf[len++] = path[i];
    buf[len] = '\0';
    return buf;
}

static void shell_show_prompt(void)
{
    term_puts(shell_cwd);
    term_puts("> ");
    shell_prompt_shown = 1;
}

/* ---- Built-in commands ---- */

static int shell_cmd_help(void)
{
    shell_puts("Built-in commands:\n");
    shell_puts("  help    - show this message\n");
    shell_puts("  clear   - clear screen\n");
    shell_puts("  cd DIR  - change directory\n");
    shell_puts("  echo .. - print arguments\n");
    shell_puts("  halt    - stop the system\n");
    shell_puts("External programs in /bin/:\n");
    shell_puts("  ls cat cp mv rm mkdir touch\n");
    shell_puts("  ps free df kill sync\n");
    shell_puts("  grep head wc tree\n");
    return 0;
}

static int shell_cmd_clear(void)
{
    term_clear();
    return 0;
}

static int shell_cmd_echo(char *args)
{
    if (args)
    {
        shell_puts(args);
    }
    shell_putchar('\n');
    return 0;
}

static int shell_cmd_cd(char *args)
{
    int i;
    char resolved[SHELL_CWD_MAX];
    struct brfs_dir_entry entries[1];

    if (!args || !args[0])
    {
        shell_cwd[0] = '/';
        shell_cwd[1] = '\0';
        shell_sync_cwd();
        return 0;
    }

    /* Handle ".." — go up one directory */
    if (args[0] == '.' && args[1] == '.' && args[2] == '\0')
    {
        int len;
        len = 0;
        while (shell_cwd[len]) len++;
        /* Remove trailing slash if not root */
        if (len > 1 && shell_cwd[len - 1] == '/')
            len--;
        /* Find last slash */
        while (len > 0 && shell_cwd[len - 1] != '/')
            len--;
        if (len <= 1)
        {
            shell_cwd[0] = '/';
            shell_cwd[1] = '\0';
        }
        else
        {
            shell_cwd[len] = '\0';
        }
        shell_sync_cwd();
        return 0;
    }

    /* Resolve path */
    shell_resolve_path(resolved, SHELL_CWD_MAX, args);

    /* Validate directory exists (try readdir) */
    if (vfs_readdir(resolved, entries, 1) < 0)
    {
        term_puts("cd: no such directory: ");
        term_puts(args);
        term_putchar('\n');
        return 1;
    }

    /* Set cwd */
    for (i = 0; resolved[i] && i < SHELL_CWD_MAX - 1; i++)
        shell_cwd[i] = resolved[i];
    shell_cwd[i] = '\0';
    shell_sync_cwd();
    return 0;
}

static void shell_print_int(int val)
{
    char buf[16];
    char tmp[16];
    int len;
    int i;

    if (val < 0)
    {
        shell_putchar('-');
        val = -val;
    }
    len = 0;
    if (val == 0)
    {
        buf[0] = '0';
        len = 1;
    }
    else
    {
        while (val > 0)
        {
            tmp[len] = '0' + (val % 10);
            val = val / 10;
            len++;
        }
        for (i = 0; i < len; i++)
            buf[i] = tmp[len - 1 - i];
    }
    buf[len] = '\0';
    shell_puts(buf);
}

/* Check if a command exists as an external program.
 * Checks:  absolute paths (/foo/bar),
 *          relative paths (./foo, dir/foo) — resolved against cwd,
 *          bare commands (ls) — looked up in /bin/ */
static int shell_has_external(const char *cmd)
{
    char path[128];
    int ci;

    if (cmd[0] == '/')
    {
        /* Absolute path — check BRFS directly (skip leading /) */
        return brfs_exists(&brfs_spi, cmd + 1);
    }

    /* Check if path contains '/' (e.g., ./foo or dir/foo) */
    for (ci = 0; cmd[ci]; ci++)
    {
        if (cmd[ci] == '/')
        {
            /* Relative path — resolve against cwd */
            shell_resolve_path(path, 128, cmd);
            if (path[0] == '/')
                return brfs_exists(&brfs_spi, path + 1);
            return brfs_exists(&brfs_spi, path);
        }
    }

    /* Bare command name — look up in /bin/ */
    path[0] = 'b'; path[1] = 'i';
    path[2] = 'n'; path[3] = '/';
    for (ci = 0; cmd[ci] && ci < 122; ci++)
        path[4 + ci] = cmd[ci];
    path[4 + ci] = '\0';

    return brfs_exists(&brfs_spi, path);
}

/* ---- Pipe temp file helpers ---- */

static int pipe_counter;

static void shell_pipe_path(char *buf, int idx)
{
    /* "/tmp/p.N" */
    buf[0] = '/'; buf[1] = 't'; buf[2] = 'm'; buf[3] = 'p';
    buf[4] = '/'; buf[5] = 'p'; buf[6] = '.';
    buf[7] = '0' + (char)idx;
    buf[8] = '\0';
}

/* ---- Run a single command (builtin or external) ---- */

/* Try to match and run a builtin. Returns 1 if matched, 0 if not. */
static int shell_try_builtin(char *cmd, char *args)
{
    if (cmd[0] == 'h' && cmd[1] == 'e' && cmd[2] == 'l' && cmd[3] == 'p' && cmd[4] == '\0')
        { shell_cmd_help(); return 1; }
    if (cmd[0] == 'c' && cmd[1] == 'l' && cmd[2] == 'e' && cmd[3] == 'a' && cmd[4] == 'r' && cmd[5] == '\0')
        { shell_cmd_clear(); return 1; }
    if (cmd[0] == 'e' && cmd[1] == 'c' && cmd[2] == 'h' && cmd[3] == 'o' && cmd[4] == '\0')
        { shell_cmd_echo(args); return 1; }
    if (cmd[0] == 'c' && cmd[1] == 'd' && cmd[2] == '\0')
        { shell_cmd_cd(args); return 1; }
    if (cmd[0] == 'h' && cmd[1] == 'a' && cmd[2] == 'l' && cmd[3] == 't' && cmd[4] == '\0')
        { term_puts("System halted.\n"); kernel_panic("user halt"); return 1; }
    return 0;
}

/* Run an external command. stdin_gfd/stdout_gfd: -1 = inherit, >= 0 = redirect */
static int shell_run_external(char *cmd, char *args, int stdin_gfd, int stdout_gfd)
{
    int pid;
    char resolved[128];
    struct proc *p;
    int exit_code;
    int j;

    /* Argument parsing */
    int argc;
    char *argv[16];
    argc = 0;
    argv[argc++] = cmd;
    if (args && args[0])
    {
        char *a;
        a = args;
        while (*a && argc < 16)
        {
            argv[argc++] = a;
            while (*a && *a != ' ') a++;
            if (*a == ' ')
            {
                *a = '\0';
                a++;
                while (*a == ' ') a++;
            }
        }
    }

    /* Path resolution:
     * - Absolute paths (/foo): use as-is
     * - Paths with / (./foo, dir/foo): resolve against cwd
     * - Bare names (ls): prepend /bin/ */
    if (cmd[0] == '/')
    {
        int ci;
        for (ci = 0; cmd[ci] && ci < 127; ci++)
            resolved[ci] = cmd[ci];
        resolved[ci] = '\0';
    }
    else
    {
        int ci;
        int has_slash;
        has_slash = 0;
        for (ci = 0; cmd[ci]; ci++)
        {
            if (cmd[ci] == '/') { has_slash = 1; break; }
        }
        if (has_slash)
        {
            shell_resolve_path(resolved, 128, cmd);
        }
        else
        {
            resolved[0] = '/';
            resolved[1] = 'b';
            resolved[2] = 'i';
            resolved[3] = 'n';
            resolved[4] = '/';
            for (ci = 0; cmd[ci] && ci < 122; ci++)
                resolved[5 + ci] = cmd[ci];
            resolved[5 + ci] = '\0';
        }
    }

    pid = proc_spawn(resolved, argc, argv);
    if (pid < 0)
    {
        term_puts(cmd);
        term_puts(": command not found\n");
        return -1;
    }

    p = proc_by_pid(pid);

    /* Apply I/O redirections before entering the program */
    if (stdin_gfd >= 0)
    {
        /* Close inherited stdin, replace with redirected fd */
        if (p->fds[0] >= 0)
            vfs_close(p->fds[0]);
        p->fds[0] = stdin_gfd;
        vfs_addref(stdin_gfd);
    }
    if (stdout_gfd >= 0)
    {
        /* Close inherited stdout, replace with redirected fd */
        if (p->fds[1] >= 0)
            vfs_close(p->fds[1]);
        p->fds[1] = stdout_gfd;
        vfs_addref(stdout_gfd);
    }

    /* Switch current process to user program */
    current_pid = pid;
    p->state = PROC_RUNNING;

    /* Enter user program (blocks until it finishes) */
    context_enter(p->saved_pc, p->saved_regs[13]);

    /* Program finished — back to kernel */
    current_pid = 0;

    /* Clean up if proc_exit wasn't called (natural return from main) */
    if (p->state != PROC_ZOMBIE)
    {
        for (j = 0; j < MAX_FDS; j++)
        {
            if (p->fds[j] >= 0)
            {
                vfs_close(p->fds[j]);
                p->fds[j] = -1;
            }
        }
        if (p->mem_base)
        {
            mem_free_region(p->mem_base, p->mem_size);
            p->mem_base = 0;
            p->mem_size = 0;
        }
        p->exit_code = (int)context_enter_retval;
        p->state = PROC_ZOMBIE;
    }

    /* Collect exit code and free process slot */
    exit_code = p->exit_code;
    p->state = PROC_FREE;

    /*
     * Safety net: close any leaked VFS and BRFS entries.
     * vfs_close_orphans() properly closes VFS entries (which also
     * closes underlying BRFS handles via ops->close).  brfs_close_all
     * catches any BRFS handles not associated with a VFS entry.
     * Phase 1 is single-foreground, so this is safe.
     */
    vfs_close_orphans();
    brfs_close_all(&brfs_spi);
    if (fs_sd_ready)
        brfs_close_all(&brfs_sd);

    kernel_ccache();

    return exit_code;
}

/* ---- Parse and split a command segment into cmd + args ---- */

static void shell_parse_segment(char *seg, char **cmd_out, char **args_out)
{
    char *cmd;
    char *args;

    cmd = seg;
    while (*cmd == ' ' || *cmd == '\t') cmd++;

    /* Find args (first space after command) */
    args = cmd;
    while (*args && *args != ' ') args++;
    if (*args == ' ')
    {
        *args = '\0';
        args++;
        while (*args == ' ') args++;
    }
    else
    {
        args = 0;
    }

    *cmd_out = cmd;
    *args_out = args;
}

/* ---- I/O redirection parsing ---- */

/*
 * Scan a command string for > and < operators, extract filenames.
 * Modifies the string in-place (null-terminates at redirect operator).
 * Supports: > file, >> file (append), < file
 */
static void shell_parse_redirects(char *str, char **redir_out, char **redir_in,
                                  int *append_flag)
{
    char *p;

    *redir_out = 0;
    *redir_in = 0;
    *append_flag = 0;

    p = str;
    while (*p)
    {
        if (*p == '>')
        {
            /* Null-terminate the command portion */
            *p = '\0';
            p++;
            if (*p == '>')
            {
                *append_flag = 1;
                p++;
            }
            while (*p == ' ') p++;
            *redir_out = p;
            /* Advance past filename */
            while (*p && *p != ' ' && *p != '<') p++;
            if (*p == ' ' || *p == '<') continue;
            break;
        }
        else if (*p == '<')
        {
            *p = '\0';
            p++;
            while (*p == ' ') p++;
            *redir_in = p;
            while (*p && *p != ' ' && *p != '>') p++;
            if (*p == ' ' || *p == '>') continue;
            break;
        }
        else
        {
            p++;
        }
    }

    /* Trim trailing spaces from the command portion */
    {
        int len;
        len = 0;
        while (str[len]) len++;
        while (len > 0 && (str[len - 1] == ' ' || str[len - 1] == '\t'))
            str[--len] = '\0';
    }
}

/* ---- Command dispatcher ---- */

static void shell_execute(char *line)
{
    char *segments[8];
    int seg_count;
    int i;
    char *p;
    char *cmd;
    char *args;

    /* Skip leading whitespace */
    p = line;
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '\0' || *p == '\n') return;

    /* Strip trailing newline */
    for (i = 0; p[i]; i++)
    {
        if (p[i] == '\n')
        {
            p[i] = '\0';
            break;
        }
    }

    /* Split at pipe '|' characters */
    seg_count = 0;
    segments[seg_count++] = p;
    for (i = 0; p[i]; i++)
    {
        if (p[i] == '|')
        {
            p[i] = '\0';
            if (seg_count < 8)
                segments[seg_count++] = &p[i + 1];
        }
    }

    /* Single command — no pipe */
    if (seg_count == 1)
    {
        char *redir_out;
        char *redir_in;
        int append_flag;
        char redir_path[SHELL_CWD_MAX];
        int out_gfd;
        int in_gfd;

        /* Parse redirects before splitting cmd/args */
        shell_parse_redirects(segments[0], &redir_out, &redir_in, &append_flag);
        shell_parse_segment(segments[0], &cmd, &args);
        if (cmd[0] == '\0') return;

        out_gfd = -1;
        in_gfd = -1;

        /* Open redirect files */
        if (redir_out && redir_out[0])
        {
            shell_resolve_path(redir_path, SHELL_CWD_MAX, redir_out);
            out_gfd = vfs_open(redir_path,
                               O_WRONLY | O_CREAT | (append_flag ? O_APPEND : 0));
            if (out_gfd < 0)
            {
                term_puts("redirect: cannot open ");
                term_puts(redir_out);
                term_putchar('\n');
                return;
            }
        }
        if (redir_in && redir_in[0])
        {
            shell_resolve_path(redir_path, SHELL_CWD_MAX, redir_in);
            in_gfd = vfs_open(redir_path, O_RDONLY);
            if (in_gfd < 0)
            {
                term_puts("redirect: cannot open ");
                term_puts(redir_in);
                term_putchar('\n');
                if (out_gfd >= 0) vfs_close(out_gfd);
                return;
            }
        }

        /* Set builtin output redirect if needed */
        if (out_gfd >= 0)
            shell_out_gfd = out_gfd;

        /* Prefer external /bin/<cmd> over builtin */
        if (shell_has_external(cmd))
        {
            shell_out_gfd = -1;
            int exit_code;
            exit_code = shell_run_external(cmd, args, in_gfd, out_gfd);
            if (exit_code > 0)
            {
                term_puts("Program exited with code ");
                shell_print_int(exit_code);
                term_putchar('\n');
            }
        }
        else if (!shell_try_builtin(cmd, args))
        {
            shell_out_gfd = -1;
            term_puts(cmd);
            term_puts(": command not found\n");
        }
        shell_out_gfd = -1;

        if (out_gfd >= 0) vfs_close(out_gfd);
        if (in_gfd >= 0) vfs_close(in_gfd);
        return;
    }

    /* Pipeline: cmd1 | cmd2 | ... | cmdN */
    {
        int pipe_gfds[8];
        char pipe_path[16];
        int si;

        /* Initialize */
        for (i = 0; i < 8; i++)
            pipe_gfds[i] = -1;

        for (si = 0; si < seg_count; si++)
        {
            int out_gfd;
            int in_gfd;
            int is_last;

            is_last = (si == seg_count - 1);
            out_gfd = -1;
            in_gfd = -1;

            /* Open output temp file (except for last segment) */
            if (!is_last)
            {
                shell_pipe_path(pipe_path, si);
                out_gfd = vfs_open(pipe_path, O_WRONLY | O_CREAT);
                if (out_gfd < 0)
                {
                    term_puts("pipe: cannot create temp file\n");
                    goto pipe_cleanup;
                }
            }

            /* Use input temp file from previous segment (except first) */
            if (si > 0)
            {
                shell_pipe_path(pipe_path, si - 1);
                in_gfd = vfs_open(pipe_path, O_RDONLY);
                if (in_gfd < 0)
                {
                    term_puts("pipe: cannot open temp file\n");
                    if (out_gfd >= 0) vfs_close(out_gfd);
                    goto pipe_cleanup;
                }
            }

            shell_parse_segment(segments[si], &cmd, &args);

            if (cmd[0] != '\0')
            {
                /* Prefer external /bin/<cmd> over builtin */
                if (shell_has_external(cmd))
                {
                    shell_out_gfd = -1;
                    shell_run_external(cmd, args, in_gfd, out_gfd);
                }
                else
                {
                    if (out_gfd >= 0)
                        shell_out_gfd = out_gfd;
                    if (!shell_try_builtin(cmd, args))
                    {
                        shell_out_gfd = -1;
                        shell_run_external(cmd, args, in_gfd, out_gfd);
                    }
                    shell_out_gfd = -1;
                }
            }

            /* Close the gfds we opened for this segment */
            if (out_gfd >= 0)
                vfs_close(out_gfd);
            if (in_gfd >= 0)
                vfs_close(in_gfd);
        }

pipe_cleanup:
        /* Clean up temp files */
        for (i = 0; i < seg_count - 1; i++)
        {
            shell_pipe_path(pipe_path, i);
            vfs_unlink(pipe_path);
        }
    }
}

/* ---- Public API ---- */

void shell_init(void)
{
    shell_cwd[0] = '/';
    shell_cwd[1] = '\0';
    shell_input_len = 0;
    shell_prompt_shown = 0;
    shell_out_gfd = -1;
    pipe_counter = 0;
    /* Ensure /tmp exists for pipe temp files */
    vfs_mkdir("/tmp");
}

void shell_tick(void)
{
    int ch;

    /* Check for Ctrl+C at the shell prompt — clear input line */
    if (ctrl_c_pending)
    {
        ctrl_c_pending = 0;
        term_puts("^C\n");
        shell_input_len = 0;
        shell_prompt_shown = 0;
        return;
    }

    if (!shell_prompt_shown)
    {
        shell_show_prompt();
    }

    /* Read available key events */
    while (hid_event_available())
    {
        ch = hid_event_read();
        if (ch < 0) break;

        if (ch == '\n' || ch == '\r')
        {
            shell_input[shell_input_len] = '\n';
            shell_input[shell_input_len + 1] = '\0';
            term_putchar('\n');

            shell_execute(shell_input);

            shell_input_len = 0;
            shell_prompt_shown = 0;
        }
        else if (ch == '\b' || ch == 0x7F)
        {
            if (shell_input_len > 0)
            {
                shell_input_len--;
                term_putchar('\b');
                term_putchar(' ');
                term_putchar('\b');
            }
        }
        else if (ch >= 0x20 && ch < 0x100)
        {
            if (shell_input_len < SHELL_INPUT_MAX - 2)
            {
                shell_input[shell_input_len] = (char)ch;
                shell_input_len++;
                term_putchar((char)ch);
            }
        }
        /* Ignore special keys for now */
    }
}
