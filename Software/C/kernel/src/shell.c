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
    term_puts("Built-in commands:\n");
    term_puts("  help    - show this message\n");
    term_puts("  clear   - clear screen\n");
    term_puts("  ls      - list directory\n");
    term_puts("  cd DIR  - change directory\n");
    term_puts("  cat F   - print file contents\n");
    term_puts("  echo .. - print arguments\n");
    term_puts("  free    - show free memory\n");
    term_puts("  ps      - show process table\n");
    term_puts("  kill N  - terminate process by PID\n");
    term_puts("  sync    - flush filesystems\n");
    term_puts("  halt    - stop the system\n");
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
        term_puts(args);
    }
    term_putchar('\n');
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
    term_puts("Free process memory: ");
    term_puts(buf);
    term_puts(" bytes\n");
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
        term_puts(name);
        if (entries[i].flags & BRFS_FLAG_DIRECTORY)
            term_putchar('/');
        term_putchar('\n');
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
        term_puts(buf);
    }
    vfs_close(gfd);
    term_putchar('\n');
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
        term_putchar('-');
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
    term_puts(buf);
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

    term_puts("PID  STATE  NAME\n");
    for (i = 0; i < MAX_PROCS; i++)
    {
        p = proc_by_pid(i);
        if (!p) continue;

        /* PID */
        if (i < 10) term_putchar(' ');
        shell_print_int(i);
        term_puts("   ");

        /* State */
        if (p->state >= 0 && p->state <= 4)
            term_puts(state_names[p->state]);
        else
            term_puts("??? ");
        term_puts("   ");

        /* Name */
        term_puts(p->name);
        term_putchar('\n');
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

    term_puts("killed pid ");
    shell_print_int(pid);
    term_putchar('\n');
    return 0;
}

static int shell_cmd_sync(void)
{
    fs_sync_all();
    term_puts("filesystems synced\n");
    return 0;
}

/* ---- Command dispatcher ---- */

static void shell_execute(char *line)
{
    char *cmd;
    char *args;
    int i;

    /* Skip leading whitespace */
    cmd = line;
    while (*cmd == ' ' || *cmd == '\t') cmd++;
    if (*cmd == '\0' || *cmd == '\n') return;

    /* Strip trailing newline */
    for (i = 0; cmd[i]; i++)
    {
        if (cmd[i] == '\n')
        {
            cmd[i] = '\0';
            break;
        }
    }

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

    /* Dispatch */
    if (cmd[0] == 'h' && cmd[1] == 'e' && cmd[2] == 'l' && cmd[3] == 'p' && cmd[4] == '\0')
        shell_cmd_help();
    else if (cmd[0] == 'c' && cmd[1] == 'l' && cmd[2] == 'e' && cmd[3] == 'a' && cmd[4] == 'r' && cmd[5] == '\0')
        shell_cmd_clear();
    else if (cmd[0] == 'e' && cmd[1] == 'c' && cmd[2] == 'h' && cmd[3] == 'o' && cmd[4] == '\0')
        shell_cmd_echo(args);
    else if (cmd[0] == 'f' && cmd[1] == 'r' && cmd[2] == 'e' && cmd[3] == 'e' && cmd[4] == '\0')
        shell_cmd_free();
    else if (cmd[0] == 'l' && cmd[1] == 's' && cmd[2] == '\0')
        shell_cmd_ls(args);
    else if (cmd[0] == 'c' && cmd[1] == 'd' && cmd[2] == '\0')
        shell_cmd_cd(args);
    else if (cmd[0] == 'c' && cmd[1] == 'a' && cmd[2] == 't' && cmd[3] == '\0')
        shell_cmd_cat(args);
    else if (cmd[0] == 's' && cmd[1] == 'y' && cmd[2] == 'n' && cmd[3] == 'c' && cmd[4] == '\0')
        shell_cmd_sync();
    else if (cmd[0] == 'p' && cmd[1] == 's' && cmd[2] == '\0')
        shell_cmd_ps();
    else if (cmd[0] == 'k' && cmd[1] == 'i' && cmd[2] == 'l' && cmd[3] == 'l' && cmd[4] == '\0')
        shell_cmd_kill(args);
    else if (cmd[0] == 'h' && cmd[1] == 'a' && cmd[2] == 'l' && cmd[3] == 't' && cmd[4] == '\0')
    {
        term_puts("System halted.\n");
        kernel_panic("user halt");
    }
    else
    {
        /* Try to run as external program */
        int pid;
        char resolved[128];

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

        pid = proc_spawn(resolved, 0, 0);
        if (pid < 0)
        {
            term_puts(cmd);
            term_puts(": command not found\n");
        }
        else
        {
            struct proc *p;
            int exit_code;
            int j;

            p = proc_by_pid(pid);

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

            if (exit_code != 0)
            {
                char buf[16];
                int len;
                int val;
                int bi;

                term_puts("Program exited with code ");
                val = exit_code;
                len = 0;
                if (val < 0)
                {
                    term_putchar('-');
                    val = -val;
                }
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
                    for (bi = 0; bi < len; bi++)
                        buf[bi] = tmp[len - 1 - bi];
                }
                buf[len] = '\0';
                term_puts(buf);
                term_putchar('\n');
            }
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
}

void shell_tick(void)
{
    int ch;

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
