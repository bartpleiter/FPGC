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
        /* Use provided path */
        for (i = 0; args[i] && i < SHELL_CWD_MAX - 1; i++)
            path[i] = args[i];
        path[i] = '\0';
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

    if (!args || !args[0])
    {
        shell_cwd[0] = '/';
        shell_cwd[1] = '\0';
        return 0;
    }

    /* Simple: set cwd directly */
    if (args[0] == '/')
    {
        for (i = 0; args[i] && i < SHELL_CWD_MAX - 1; i++)
            shell_cwd[i] = args[i];
        shell_cwd[i] = '\0';
    }
    else
    {
        /* Append to cwd */
        int len;
        len = 0;
        while (shell_cwd[len]) len++;
        if (len > 1 && shell_cwd[len - 1] != '/')
        {
            shell_cwd[len] = '/';
            len++;
        }
        for (i = 0; args[i] && len < SHELL_CWD_MAX - 1; i++)
        {
            shell_cwd[len] = args[i];
            len++;
        }
        shell_cwd[len] = '\0';
    }
    return 0;
}

static int shell_cmd_cat(char *args)
{
    int gfd;
    char buf[256];
    int n;

    if (!args || !args[0])
    {
        term_puts("cat: missing file argument\n");
        return 1;
    }

    gfd = vfs_open(args, O_RDONLY);
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
    else if (cmd[0] == 'h' && cmd[1] == 'a' && cmd[2] == 'l' && cmd[3] == 't' && cmd[4] == '\0')
    {
        term_puts("System halted.\n");
        kernel_panic("user halt");
    }
    else
    {
        /* Try to run as external program */
        int pid;
        pid = proc_spawn(cmd, 0, 0);
        if (pid < 0)
        {
            term_puts(cmd);
            term_puts(": command not found\n");
        }
        else
        {
            /* Wait for it to finish */
            proc_waitpid(pid);
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
