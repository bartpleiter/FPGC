/*
 * shell_script.c \u2014 in-kernel #!/bin/sh script interpreter.
 *
 * Reads a script file line-by-line through the VFS, expands $0..$N
 * positional parameters, runs each line through the same lex/parse/
 * execute pipeline as interactive input. Comments and blank lines
 * are skipped. `exit [n]` aborts the script with code n.
 *
 * Only #!/bin/sh (or just #!sh) shebangs are handled in-kernel; other
 * interpreters would need to be exec'd as separate binaries (no other
 * interpreters exist on the system in v2.0).
 *
 * Exit-on-error: a non-zero exit from any command terminates the
 * script with that code (this matches `set -e` semantics).
 */

#include "bdos.h"

static int is_sh_shebang(const char *line)
{
    /* Accept "#!/bin/sh", "#! /bin/sh", "#!/bin/sh -…", "#!sh", etc. */
    const char *p = line;
    if (p[0] != '#' || p[1] != '!') return 0;
    p += 2;
    while (*p == ' ' || *p == '\t') p++;

    /* Strip a leading directory component once. */
    while (*p && *p != ' ' && *p != '\t' && *p != '\n') {
        if (*p == '/') { p++; continue; }
        /* Compare interpreter basename token to "sh". */
        if (p[0] == 's' && p[1] == 'h' &&
            (p[2] == 0 || p[2] == ' ' || p[2] == '\t' || p[2] == '\n'))
            return 1;
        p++;
    }
    return 0;
}

/* Read one line from fd into buf. Returns:
 *   > 0  number of bytes (including no newline at EOF)
 *   = 0  EOF
 *   < 0  error or line too long */
static int read_line(int fd, char *buf, int max)
{
    int n = 0;
    char c;
    int  r;
    while (n < max - 1) {
        r = bdos_vfs_read(fd, &c, 1);
        if (r == 0) break;
        if (r < 0)  return -1;
        if (c == '\n') break;
        if (c == '\r') continue;
        if (c == 0)    break;   /* NUL = end-of-text */
        buf[n++] = c;
    }
    buf[n] = 0;
    return n;
}

int bdos_shell_run_script(const char *path,
                          int script_argc, char **script_argv)
{
    int  fd;
    char line[BDOS_SHELL_INPUT_MAX];
    char expanded[BDOS_SHELL_INPUT_MAX * 2];
    char store   [BDOS_SHELL_INPUT_MAX * 2];
    sh_tok_t   toks[BDOS_SHELL_TOK_MAX];
    sh_chain_t chain;
    int  n;
    int  rc = 0;
    int  first_line = 1;

    fd = bdos_vfs_open(path, BDOS_O_RDONLY);
    if (fd < 0) {
        term2_puts("script: cannot open ");
        term2_puts(path);
        term2_putchar('\n');
        return 1;
    }

    while (1) {
        n = read_line(fd, line, sizeof(line));
        if (n < 0) {
            term2_puts("script: read error\n");
            rc = 1;
            break;
        }
        if (n == 0 && line[0] == 0) break;   /* EOF */

        if (first_line) {
            first_line = 0;
            if (line[0] == '#' && line[1] == '!') {
                if (!is_sh_shebang(line)) {
                    term2_puts("script: only #!/bin/sh shebangs supported\n");
                    rc = 1;
                    break;
                }
                continue;
            }
        }

        /* Skip blanks and full-line comments quickly. */
        {
            const char *p = line;
            while (*p == ' ' || *p == '\t') p++;
            if (*p == 0 || *p == '#') continue;
        }

        if (bdos_shell_expand(line, expanded, sizeof(expanded),
                              script_argc, script_argv) < 0) {
            term2_puts("script: bad quoting / overflow\n");
            rc = 1;
            break;
        }

        n = bdos_shell_lex(expanded, toks, BDOS_SHELL_TOK_MAX,
                           store, sizeof(store));
        if (n < 0) {
            term2_puts("script: lex error\n");
            rc = 1;
            break;
        }
        if (n == 0) continue;

        if (bdos_shell_parse(toks, &chain) < 0) { rc = 1; break; }

        rc = bdos_shell_run_chain(&chain);
        bdos_shell_last_exit = rc;
        if (rc != 0) break;   /* set -e semantics */
    }

    bdos_vfs_close(fd);
    return rc;
}
