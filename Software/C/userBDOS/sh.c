/*
 * sh.c — /bin/sh for BDOS v4.
 *
 * Userland Bourne-style shell. Port of the kernel shell (shell.c)
 * to use syscalls instead of direct kernel APIs.
 *
 * Features:
 *   - Built-in commands: help, clear, echo, cd, pwd, halt (exit)
 *   - External command execution with path resolution
 *   - I/O redirection: >, >>, <
 *   - Pipes via temp files: cmd1 | cmd2 | ...
 *   - Input via cooked-mode TTY (kernel handles line editing)
 */
#include <syscall.h>
#include <string.h>
#include <stddef.h>

#define INPUT_MAX   256
#define CWD_MAX     128
#define PATH_MAX    128
#define ARGV_MAX    16
#define PIPE_MAX    8
#define SAVE_FD     10  /* fd used to save stdout/stdin during redirects */
#define SAVE_FD_IN  11  /* fd used to save stdin during redirects */

static char input_buf[INPUT_MAX];
static char cwd[CWD_MAX];

/* ---- Output helpers ---- */

static void puts_fd(int fd, const char *s)
{
    int len;
    len = 0;
    while (s[len]) len++;
    sys_write(fd, s, len);
}

static void putchar_fd(int fd, int ch)
{
    char c;
    c = (char)ch;
    sys_write(fd, &c, 1);
}

static void print_int(int val)
{
    char buf[16];
    char tmp[16];
    int len;
    int i;

    if (val < 0)
    {
        sys_putc('-');
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
    sys_putstr(buf);
}

/* ---- Path resolution ---- */

static void resolve_path(char *out, int outsize, const char *path)
{
    int i;
    int len;

    if (path[0] == '/')
    {
        for (i = 0; path[i] && i < outsize - 1; i++)
            out[i] = path[i];
        out[i] = '\0';
        return;
    }

    /* Relative path: prepend cwd */
    len = 0;
    for (i = 0; cwd[i] && len < outsize - 1; i++)
        out[len++] = cwd[i];
    if (len > 1 && out[len - 1] != '/' && len < outsize - 1)
        out[len++] = '/';
    for (i = 0; path[i] && len < outsize - 1; i++)
        out[len++] = path[i];
    out[len] = '\0';
}

/* Check if a command exists as an external program */
static int has_external(const char *cmd)
{
    char path[PATH_MAX];
    int ci;

    if (cmd[0] == '/')
        return (sys_stat(cmd, NULL) == 0);

    /* Check if path contains '/' (relative path) */
    for (ci = 0; cmd[ci]; ci++)
    {
        if (cmd[ci] == '/')
        {
            resolve_path(path, PATH_MAX, cmd);
            return (sys_stat(path, NULL) == 0);
        }
    }

    /* Bare command: check /bin/<cmd> */
    path[0] = '/'; path[1] = 'b'; path[2] = 'i';
    path[3] = 'n'; path[4] = '/';
    for (ci = 0; cmd[ci] && ci < PATH_MAX - 6; ci++)
        path[5 + ci] = cmd[ci];
    path[5 + ci] = '\0';

    return (sys_stat(path, NULL) == 0);
}

/* ---- Built-in commands ---- */

static int cmd_help(void)
{
    sys_putstr("Built-in commands:\n");
    sys_putstr("  help    - show this message\n");
    sys_putstr("  clear   - clear screen\n");
    sys_putstr("  cd DIR  - change directory\n");
    sys_putstr("  pwd     - print working directory\n");
    sys_putstr("  echo .. - print arguments\n");
    sys_putstr("  exit    - exit shell\n");
    sys_putstr("  halt    - stop the system\n");
    sys_putstr("External programs in /bin/:\n");
    sys_putstr("  ls cat cp mv rm mkdir touch\n");
    sys_putstr("  ps free df kill sync\n");
    sys_putstr("  grep head wc tree\n");
    return 0;
}

static int cmd_clear(void)
{
    /* ANSI clear screen + cursor home */
    sys_putstr("\x1b[2J\x1b[H");
    return 0;
}

static int cmd_echo(char *args)
{
    if (args)
        sys_putstr(args);
    sys_putc('\n');
    return 0;
}

static int cmd_pwd(void)
{
    sys_putstr(cwd);
    sys_putc('\n');
    return 0;
}

static int cmd_cd(char *args)
{
    char resolved[CWD_MAX];

    if (!args || !args[0])
    {
        sys_chdir("/");
        sys_getcwd(cwd, CWD_MAX);
        return 0;
    }

    /* Handle ".." */
    if (args[0] == '.' && args[1] == '.' && args[2] == '\0')
    {
        int len;
        len = 0;
        while (cwd[len]) len++;
        if (len > 1 && cwd[len - 1] == '/')
            len--;
        while (len > 0 && cwd[len - 1] != '/')
            len--;
        if (len <= 1)
        {
            cwd[0] = '/';
            cwd[1] = '\0';
        }
        else
        {
            cwd[len] = '\0';
        }
        sys_chdir(cwd);
        return 0;
    }

    /* Resolve and validate */
    resolve_path(resolved, CWD_MAX, args);

    /* Try readdir to validate it's a directory */
    {
        char entry_buf[64];
        if (sys_readdir(resolved, entry_buf, 1) < 0)
        {
            sys_putstr("cd: no such directory: ");
            sys_putstr(args);
            sys_putc('\n');
            return 1;
        }
    }

    /* Update cwd */
    {
        int i;
        for (i = 0; resolved[i] && i < CWD_MAX - 1; i++)
            cwd[i] = resolved[i];
        cwd[i] = '\0';
    }
    sys_chdir(cwd);
    return 0;
}

/* ---- Command parsing ---- */

static void parse_segment(char *seg, char **cmd_out, char **args_out)
{
    char *c;
    char *a;

    c = seg;
    while (*c == ' ' || *c == '\t') c++;

    a = c;
    while (*a && *a != ' ') a++;
    if (*a == ' ')
    {
        *a = '\0';
        a++;
        while (*a == ' ') a++;
    }
    else
    {
        a = NULL;
    }

    *cmd_out = c;
    *args_out = a;
}

/* Parse >, >>, < from a command string */
static void parse_redirects(char *str, char **redir_out, char **redir_in,
                            int *append_flag)
{
    char *p;

    *redir_out = NULL;
    *redir_in = NULL;
    *append_flag = 0;

    p = str;
    while (*p)
    {
        if (*p == '>')
        {
            *p = '\0';
            p++;
            if (*p == '>')
            {
                *append_flag = 1;
                p++;
            }
            while (*p == ' ') p++;
            *redir_out = p;
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

    /* Trim trailing spaces from command */
    {
        int len;
        len = 0;
        while (str[len]) len++;
        while (len > 0 && (str[len - 1] == ' ' || str[len - 1] == '\t'))
            str[--len] = '\0';
    }
}

/* ---- External command execution ---- */

static int run_external(char *cmd, char *args, int redir_stdin, int redir_stdout)
{
    char resolved[PATH_MAX];
    int argc;
    char *argv[ARGV_MAX];
    int pid;
    int exit_code;

    /* Build argv */
    argc = 0;
    argv[argc++] = cmd;
    if (args && args[0])
    {
        char *a;
        a = args;
        while (*a && argc < ARGV_MAX)
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

    /* Path resolution */
    if (cmd[0] == '/')
    {
        int ci;
        for (ci = 0; cmd[ci] && ci < PATH_MAX - 1; ci++)
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
            resolve_path(resolved, PATH_MAX, cmd);
        }
        else
        {
            resolved[0] = '/';
            resolved[1] = 'b';
            resolved[2] = 'i';
            resolved[3] = 'n';
            resolved[4] = '/';
            for (ci = 0; cmd[ci] && ci < PATH_MAX - 6; ci++)
                resolved[5 + ci] = cmd[ci];
            resolved[5 + ci] = '\0';
        }
    }

    /* Set up I/O redirection before exec */
    if (redir_stdout)
    {
        sys_dup2(1, SAVE_FD);  /* save stdout */
        sys_dup2(redir_stdout, 1);
        sys_close(redir_stdout);
    }
    if (redir_stdin)
    {
        sys_dup2(0, SAVE_FD_IN);  /* save stdin */
        sys_dup2(redir_stdin, 0);
        sys_close(redir_stdin);
    }

    /* Spawn child */
    pid = sys_exec(resolved, argc, (const char **)argv);
    if (pid < 0)
    {
        /* Restore fds before error message */
        if (redir_stdout)
        {
            sys_dup2(SAVE_FD, 1);
            sys_close(SAVE_FD);
        }
        if (redir_stdin)
        {
            sys_dup2(SAVE_FD_IN, 0);
            sys_close(SAVE_FD_IN);
        }
        sys_putstr(cmd);
        sys_putstr(": command not found\n");
        return -1;
    }

    /* Wait for child to finish */
    exit_code = sys_waitpid(pid);

    /* Restore I/O */
    if (redir_stdout)
    {
        sys_dup2(SAVE_FD, 1);
        sys_close(SAVE_FD);
    }
    if (redir_stdin)
    {
        sys_dup2(SAVE_FD_IN, 0);
        sys_close(SAVE_FD_IN);
    }

    return exit_code;
}

/* Try to match and run a builtin. Returns 1 if matched. */
static int try_builtin(char *cmd, char *args, int out_fd)
{
    int saved;
    int matched;

    saved = -1;
    matched = 0;

    /* Redirect builtin output if needed */
    if (out_fd > 0)
    {
        sys_dup2(1, SAVE_FD);
        sys_dup2(out_fd, 1);
        saved = SAVE_FD;
    }

    if (strcmp(cmd, "help") == 0)      { cmd_help(); matched = 1; }
    else if (strcmp(cmd, "clear") == 0) { cmd_clear(); matched = 1; }
    else if (strcmp(cmd, "echo") == 0)  { cmd_echo(args); matched = 1; }
    else if (strcmp(cmd, "cd") == 0)    { cmd_cd(args); matched = 1; }
    else if (strcmp(cmd, "pwd") == 0)   { cmd_pwd(); matched = 1; }
    else if (strcmp(cmd, "exit") == 0)  { sys_exit(0); }
    else if (strcmp(cmd, "halt") == 0)
    {
        sys_putstr("System halted.\n");
        sys_exit(0);
    }

    if (saved >= 0)
    {
        sys_dup2(saved, 1);
        sys_close(saved);
    }

    return matched;
}

/* ---- Pipe temp file helpers ---- */

static void pipe_path(char *buf, int idx)
{
    buf[0] = '/'; buf[1] = 't'; buf[2] = 'm'; buf[3] = 'p';
    buf[4] = '/'; buf[5] = 'p'; buf[6] = '.';
    buf[7] = '0' + (char)idx;
    buf[8] = '\0';
}

/* ---- Main command dispatcher ---- */

static void execute(char *line)
{
    char *segments[PIPE_MAX];
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

    /* Split at pipe '|' */
    seg_count = 0;
    segments[seg_count++] = p;
    for (i = 0; p[i]; i++)
    {
        if (p[i] == '|')
        {
            p[i] = '\0';
            if (seg_count < PIPE_MAX)
                segments[seg_count++] = &p[i + 1];
        }
    }

    /* Single command (no pipe) */
    if (seg_count == 1)
    {
        char *redir_out;
        char *redir_in;
        int append_flag;
        char redir_path[CWD_MAX];
        int out_fd;
        int in_fd;

        parse_redirects(segments[0], &redir_out, &redir_in, &append_flag);
        parse_segment(segments[0], &cmd, &args);
        if (cmd[0] == '\0') return;

        out_fd = 0;
        in_fd = 0;

        /* Open redirect files */
        if (redir_out && redir_out[0])
        {
            resolve_path(redir_path, CWD_MAX, redir_out);
            out_fd = sys_open(redir_path,
                              O_WRONLY | O_CREAT | (append_flag ? O_APPEND : 0));
            if (out_fd < 0)
            {
                sys_putstr("redirect: cannot open ");
                sys_putstr(redir_out);
                sys_putc('\n');
                return;
            }
        }
        if (redir_in && redir_in[0])
        {
            resolve_path(redir_path, CWD_MAX, redir_in);
            in_fd = sys_open(redir_path, O_RDONLY);
            if (in_fd < 0)
            {
                sys_putstr("redirect: cannot open ");
                sys_putstr(redir_in);
                sys_putc('\n');
                if (out_fd > 0) sys_close(out_fd);
                return;
            }
        }

        /* Prefer external over builtin */
        if (has_external(cmd))
        {
            int exit_code;
            exit_code = run_external(cmd, args, in_fd, out_fd);
            if (exit_code > 0)
            {
                sys_putstr("Program exited with code ");
                print_int(exit_code);
                sys_putc('\n');
            }
        }
        else if (!try_builtin(cmd, args, out_fd))
        {
            sys_putstr(cmd);
            sys_putstr(": command not found\n");
        }

        if (out_fd > 0) sys_close(out_fd);
        if (in_fd > 0) sys_close(in_fd);
        return;
    }

    /* Pipeline: cmd1 | cmd2 | ... | cmdN */
    {
        char pp[16];
        int si;

        for (si = 0; si < seg_count; si++)
        {
            int out_fd;
            int in_fd;
            int is_last;

            is_last = (si == seg_count - 1);
            out_fd = 0;
            in_fd = 0;

            /* Open output temp file (except last) */
            if (!is_last)
            {
                pipe_path(pp, si);
                out_fd = sys_open(pp, O_WRONLY | O_CREAT);
                if (out_fd < 0)
                {
                    sys_putstr("pipe: cannot create temp file\n");
                    goto pipe_cleanup;
                }
            }

            /* Open input temp file from previous (except first) */
            if (si > 0)
            {
                pipe_path(pp, si - 1);
                in_fd = sys_open(pp, O_RDONLY);
                if (in_fd < 0)
                {
                    sys_putstr("pipe: cannot open temp file\n");
                    if (out_fd > 0) sys_close(out_fd);
                    goto pipe_cleanup;
                }
            }

            parse_segment(segments[si], &cmd, &args);

            if (cmd[0] != '\0')
            {
                if (has_external(cmd))
                {
                    run_external(cmd, args, in_fd, out_fd);
                }
                else if (!try_builtin(cmd, args, out_fd))
                {
                    run_external(cmd, args, in_fd, out_fd);
                }
            }

            if (out_fd > 0) sys_close(out_fd);
            if (in_fd > 0) sys_close(in_fd);
        }

pipe_cleanup:
        /* Clean up temp files */
        for (i = 0; i < seg_count - 1; i++)
        {
            pipe_path(pp, i);
            sys_unlink(pp);
        }
    }
}

/* ---- Read input line (cooked mode: kernel handles line editing) ---- */

static int read_line(void)
{
    int n;

    while (1)
    {
        n = sys_read(0, input_buf, INPUT_MAX - 1);
        if (n > 0)
        {
            input_buf[n] = '\0';
            return n;
        }
        sys_sleep(10);
    }
}

/* ---- Main ---- */

int main(void)
{
    /* Ensure /tmp exists for pipe temp files */
    sys_mkdir("/tmp");

    /* Initialize cwd */
    sys_getcwd(cwd, CWD_MAX);

    while (1)
    {
        /* Show prompt */
        sys_putstr(cwd);
        sys_putstr("> ");

        /* Read a line (blocks via poll+sleep until Enter) */
        read_line();

        /* Execute */
        execute(input_buf);

        /* Refresh cwd (may have changed via cd) */
        sys_getcwd(cwd, CWD_MAX);
    }
}
