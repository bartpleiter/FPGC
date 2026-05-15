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
    shell_puts("  ls      - list directory\n");
    shell_puts("  cd DIR  - change directory\n");
    shell_puts("  cat F   - print file contents\n");
    shell_puts("  echo .. - print arguments\n");
    shell_puts("  free    - show free memory\n");
    shell_puts("  ps      - show process table\n");
    shell_puts("  kill N  - terminate process by PID\n");
    shell_puts("  sync    - flush filesystems\n");
    shell_puts("  halt    - stop the system\n");
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

static int shell_cmd_free(void)
{
    unsigned int free_bytes;
    char buf[16];
    free_bytes = mem_free_total();
    /* Simple decimal conversion */
    {
        int i;
        int len;
        unsigned int val;
        val = free_bytes;
        len = 0;
        if (val == 0)
        {
            buf[0] = '0';
            len = 1;
        }
        else
        {
            char tmp[16];
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
    }
    shell_puts("Free process memory: ");
    shell_puts(buf);
    shell_puts(" bytes\n");
    return 0;
}

static int shell_cmd_ls(char *args)
{
    char path[SHELL_CWD_MAX];
    struct brfs_dir_entry entries[32];
    int result;
    int i;

    if (args && args[0])
    {
        shell_resolve_path(path, SHELL_CWD_MAX, args);
    }
    else
    {
        /* Use cwd */
        for (i = 0; shell_cwd[i] && i < SHELL_CWD_MAX - 1; i++)
            path[i] = shell_cwd[i];
        path[i] = '\0';
    }

    result = vfs_readdir(path, entries, 32);
    if (result < 0)
    {
        term_puts("ls: cannot list directory\n");
        return 1;
    }

    /* result is number of entries returned */
    for (i = 0; i < result; i++)
    {
        char name[17];
        brfs_decompress_string(name, entries[i].filename, 4);
        name[16] = '\0';
        shell_puts(name);
        if (entries[i].flags & BRFS_FLAG_DIRECTORY)
            shell_putchar('/');
        shell_putchar('\n');
    }

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
    return 0;
}

static int shell_cmd_cat(char *args)
{
    int gfd;
    char buf[256];
    char resolved[SHELL_CWD_MAX];
    int n;

    if (!args || !args[0])
    {
        term_puts("cat: missing file argument\n");
        return 1;
    }

    shell_resolve_path(resolved, SHELL_CWD_MAX, args);
    gfd = vfs_open(resolved, O_RDONLY);
    if (gfd < 0)
    {
        term_puts("cat: cannot open file\n");
        return 1;
    }

    while ((n = vfs_read(gfd, buf, 255)) > 0)
    {
        buf[n] = '\0';
        shell_puts(buf);
    }
    vfs_close(gfd);
    shell_putchar('\n');
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

static int shell_cmd_ps(void)
{
    int i;
    struct proc *p;
    const char *state_names[5];

    state_names[0] = "free";
    state_names[1] = "run ";
    state_names[2] = "rdy ";
    state_names[3] = "blk ";
    state_names[4] = "zomb";

    shell_puts("PID  STATE  NAME\n");
    for (i = 0; i < MAX_PROCS; i++)
    {
        p = proc_by_pid(i);
        if (!p) continue;

        /* PID */
        if (i < 10) shell_putchar(' ');
        shell_print_int(i);
        shell_puts("   ");

        /* State */
        if (p->state >= 0 && p->state <= 4)
            shell_puts(state_names[p->state]);
        else
            shell_puts("??? ");
        shell_puts("   ");

        /* Name */
        shell_puts(p->name);
        shell_putchar('\n');
    }
    return 0;
}

static int shell_cmd_kill(char *args)
{
    int pid;
    int i;
    struct proc *p;

    if (!args || !args[0])
    {
        term_puts("kill: usage: kill <pid>\n");
        return 1;
    }

    /* Parse PID */
    pid = 0;
    for (i = 0; args[i] >= '0' && args[i] <= '9'; i++)
        pid = pid * 10 + (args[i] - '0');

    if (i == 0 || pid == 0)
    {
        term_puts("kill: invalid pid\n");
        return 1;
    }

    p = proc_by_pid(pid);
    if (!p)
    {
        term_puts("kill: no such process\n");
        return 1;
    }

    /* Clean up the process */
    for (i = 0; i < MAX_FDS; i++)
    {
        if (p->fds[i] >= 0)
        {
            vfs_close(p->fds[i]);
            p->fds[i] = -1;
        }
    }
    if (p->mem_base)
    {
        mem_free_region(p->mem_base, p->mem_size);
        p->mem_base = 0;
        p->mem_size = 0;
    }
    p->state = PROC_FREE;

    shell_puts("killed pid ");
    shell_print_int(pid);
    shell_putchar('\n');
    return 0;
}

static int shell_cmd_sync(void)
{
    fs_sync_all();
    shell_puts("filesystems synced\n");
    return 0;
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
    if (cmd[0] == 'f' && cmd[1] == 'r' && cmd[2] == 'e' && cmd[3] == 'e' && cmd[4] == '\0')
        { shell_cmd_free(); return 1; }
    if (cmd[0] == 'l' && cmd[1] == 's' && cmd[2] == '\0')
        { shell_cmd_ls(args); return 1; }
    if (cmd[0] == 'c' && cmd[1] == 'd' && cmd[2] == '\0')
        { shell_cmd_cd(args); return 1; }
    if (cmd[0] == 'c' && cmd[1] == 'a' && cmd[2] == 't' && cmd[3] == '\0')
        { shell_cmd_cat(args); return 1; }
    if (cmd[0] == 's' && cmd[1] == 'y' && cmd[2] == 'n' && cmd[3] == 'c' && cmd[4] == '\0')
        { shell_cmd_sync(); return 1; }
    if (cmd[0] == 'p' && cmd[1] == 's' && cmd[2] == '\0')
        { shell_cmd_ps(); return 1; }
    if (cmd[0] == 'k' && cmd[1] == 'i' && cmd[2] == 'l' && cmd[3] == 'l' && cmd[4] == '\0')
        { shell_cmd_kill(args); return 1; }
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

    /* Path resolution: /bin/<cmd> for non-absolute paths */
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
        resolved[0] = '/';
        resolved[1] = 'b';
        resolved[2] = 'i';
        resolved[3] = 'n';
        resolved[4] = '/';
        for (ci = 0; cmd[ci] && ci < 122; ci++)
            resolved[5 + ci] = cmd[ci];
        resolved[5 + ci] = '\0';
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
     * Safety net: close any BRFS files that survived the per-fd
     * cleanup. This catches leaked handles when proc_exit didn't
     * run (crash) or when the VFS→BRFS close path failed.
     * Phase 1 is single-foreground, so this is safe.
     */
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

        if (!shell_try_builtin(cmd, args))
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
                /* Try builtin first */
                if (out_gfd >= 0)
                    shell_out_gfd = out_gfd;
                /* Note: builtins don't use stdin redirection */
                if (!shell_try_builtin(cmd, args))
                {
                    shell_out_gfd = -1;
                    shell_run_external(cmd, args, in_gfd, out_gfd);
                }
                shell_out_gfd = -1;
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
