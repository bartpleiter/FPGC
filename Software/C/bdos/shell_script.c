/*
 * shell_script.c - in-kernel #!/bin/sh script interpreter.
 *
 * Slurps the entire script file into a heap-allocated buffer up front
 * (via bdos_heap_mark / bdos_heap_alloc / bdos_heap_release), closes
 * the file descriptor immediately, then iterates line-by-line.
 *
 * Slurping is required because brfs_close_all() runs after every user
 * program exits (see slot.c), which would invalidate any fd we held
 * open across child execution.
 *
 * Only #!/bin/sh (or just #!sh) shebangs are handled in-kernel; other
 * interpreters would need to be exec'd as separate binaries.
 *
 * Exit-on-error: a non-zero exit from any command terminates the
 * script with that code (this matches `set -e` semantics).
 */

#include "bdos.h"

/* Hard cap on script size (bytes). 32 KiB is plenty for any sane
 * shell script the device runs (cc.sh is ~2.5 KiB). */
#define BDOS_SCRIPT_MAX_BYTES (32u * 1024u)

static int is_sh_shebang(const char *line)
{
    /* Accept "#!/bin/sh", "#! /bin/sh", "#!/bin/sh -...", "#!sh", etc. */
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

/* Copy one line out of the slurped buffer into `buf` and return how
 * many source bytes were consumed (including the trailing '\n' or
 * stopping NUL). Returns 0 at end-of-buffer. Strips '\r'. Returns -1
 * on overflow. */
static int copy_line(const char *src, int remaining, char *buf, int max)
{
    int  in  = 0;
    int  out = 0;
    char c;
    while (in < remaining) {
        c = src[in++];
        if (c == 0)    { buf[out] = 0; return in; } /* NUL = end-of-text */
        if (c == '\n') { buf[out] = 0; return in; }
        if (c == '\r') continue;
        if (out >= max - 1) return -1;
        buf[out++] = c;
    }
    buf[out] = 0;
    return in;
}

int bdos_shell_run_script(const char *path,
                          int script_argc, char **script_argv)
{
    int            fd;
    int            file_size;
    int            n_read;
    int            pos;
    int            consumed;
    int            n;
    int            rc = 0;
    int            first_line = 1;
    unsigned int   heap_mark;
    unsigned int   alloc_words;
    char          *script_buf;
    char           line[BDOS_SHELL_INPUT_MAX];
    char           expanded[BDOS_SHELL_INPUT_MAX * 2];
    char           store   [BDOS_SHELL_INPUT_MAX * 2];
    sh_tok_t       toks[BDOS_SHELL_TOK_MAX];
    sh_chain_t     chain;

    fd = bdos_vfs_open(path, BDOS_O_RDONLY);
    if (fd < 0) {
        term_puts("script: cannot open ");
        term_puts(path);
        term_putchar('\n');
        return 1;
    }

    file_size = bdos_vfs_lseek(fd, 0, BDOS_SEEK_END);
    if (file_size < 0 || file_size > (int)BDOS_SCRIPT_MAX_BYTES) {
        bdos_vfs_close(fd);
        if (file_size > (int)BDOS_SCRIPT_MAX_BYTES)
            term_puts("script: too large (max 32 KiB)\n");
        else
            term_puts("script: cannot determine size\n");
        return 1;
    }
    if (file_size == 0) {
        bdos_vfs_close(fd);
        return 0;
    }
    (void)bdos_vfs_lseek(fd, 0, BDOS_SEEK_SET);

    heap_mark   = bdos_heap_mark();
    alloc_words = (unsigned int)file_size;
    script_buf  = (char *)bdos_heap_alloc(alloc_words);
    if (!script_buf) {
        bdos_vfs_close(fd);
        term_puts("script: out of memory\n");
        return 1;
    }

    n_read = bdos_vfs_read(fd, script_buf, file_size);
    bdos_vfs_close(fd);
    if (n_read != file_size) {
        bdos_heap_release(heap_mark);
        term_puts("script: short read\n");
        return 1;
    }

    pos = 0;
    while (pos < n_read) {
        consumed = copy_line(script_buf + pos, n_read - pos,
                             line, sizeof(line));
        if (consumed <= 0) {
            if (consumed < 0) {
                term_puts("script: line too long\n");
                rc = 1;
            }
            break;
        }
        pos += consumed;

        if (first_line) {
            first_line = 0;
            if (line[0] == '#' && line[1] == '!') {
                if (!is_sh_shebang(line)) {
                    term_puts("script: only #!/bin/sh shebangs supported\n");
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
            term_puts("script: bad quoting / overflow\n");
            rc = 1;
            break;
        }

        n = bdos_shell_lex(expanded, toks, BDOS_SHELL_TOK_MAX,
                           store, sizeof(store));
        if (n < 0) {
            term_puts("script: lex error\n");
            rc = 1;
            break;
        }
        if (n == 0) continue;

        if (bdos_shell_parse(toks, &chain) < 0) {
            term_puts("script: parse error\n");
            rc = 1;
            break;
        }

        rc = bdos_shell_run_chain(&chain);
        bdos_shell_last_exit = rc;
        if (rc != 0) break;   /* set -e semantics */
    }

    bdos_heap_release(heap_mark);
    return rc;
}
