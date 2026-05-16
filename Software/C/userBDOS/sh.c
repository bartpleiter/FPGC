/*
 * sh.c — /bin/sh for BDOS v4.
 *
 * Bourne-style shell with:
 *   - Variable expansion: $VAR, ${VAR}, $?, $#, $0-$9
 *   - Quoting: 'single', "double" (with $-expansion), \escape
 *   - Tokenizer: words, operators (<, >, >>, |, &&, ||, ;), comments
 *   - Parser: chain-of-pipelines AST
 *   - Pipes via temp files, I/O redirection (>, >>, <)
 *   - Built-in commands: help, clear, echo, cd, pwd, exit, halt,
 *     export, set, unset, env, true, false, test/[
 *   - External command execution with /bin/ path resolution
 *   - Variable assignment: NAME=value
 */
#include <syscall.h>
#include <string.h>
#include <stddef.h>

/* ---- Tunables ---- */

#define INPUT_MAX       512
#define CWD_MAX         128
#define PATH_MAX        128
#define EXPAND_MAX      1024
#define ARGV_MAX        32
#define PIPE_MAX        4
#define CHAIN_MAX       8
#define TOK_MAX         64
#define TOK_STORE_SIZE  1024
#define VAR_MAX         32
#define VAR_NAME_LEN    16
#define VAR_VALUE_LEN   96
#define SAVE_FD         10
#define SAVE_FD_IN      11
#define SCRIPT_FILE_MAX  8192   /* max script file size */
#define SCRIPT_LINES_MAX 128    /* max lines in a script */

/* Positional parameters for script execution ($0-$9, $#) */
static char  pos_param_store[10][PATH_MAX];
static int   pos_param_count;
static int   in_script;          /* 1 while executing a script file */

/* ---- Token types ---- */

#define TOK_WORD          1
#define TOK_REDIR_IN      2   /* <  */
#define TOK_REDIR_OUT     3   /* >  */
#define TOK_REDIR_APPEND  4   /* >> */
#define TOK_PIPE          5   /* |  */
#define TOK_AND           6   /* && */
#define TOK_OR            7   /* || */
#define TOK_SEMI          8   /* ;  */
#define TOK_END           0

/* Chain operator codes */
#define OP_END   0
#define OP_AND   1
#define OP_OR    2
#define OP_SEMI  3

/* ---- Data structures ---- */

typedef struct {
    int   type;
    char *text;    /* points into tok_store for WORDs; NULL for operators */
} sh_tok_t;

typedef struct {
    int   argc;
    char *argv[ARGV_MAX];
    char *redir_in;
    char *redir_out;
    char *redir_append;
} sh_cmd_t;

typedef struct {
    int      n_cmds;
    sh_cmd_t cmds[PIPE_MAX];
} sh_pipeline_t;

typedef struct {
    int           n_pipes;
    sh_pipeline_t pipes[CHAIN_MAX];
    int           ops[CHAIN_MAX];   /* operator AFTER pipes[i] */
} sh_chain_t;

typedef struct {
    int  in_use;
    int  exported;
    char name[VAR_NAME_LEN];
    char value[VAR_VALUE_LEN];
} sh_var_t;

/* ---- Global state ---- */

static char input_buf[INPUT_MAX];
static char expand_buf[EXPAND_MAX];
static char cwd[CWD_MAX];
static int  last_exit;             /* $? */

/* Tokenizer storage */
static sh_tok_t tok_buf[TOK_MAX];
static char     tok_store[TOK_STORE_SIZE];

/* Variable table */
static sh_var_t var_table[VAR_MAX];

/* ================================================================
 *                     STRING HELPERS
 * ================================================================ */

static int sh_strlen(const char *s)
{
    int n;
    n = 0;
    while (s[n]) n++;
    return n;
}

static int sh_streq(const char *a, const char *b)
{
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

static void sh_strncpy(char *dst, const char *src, int max)
{
    int i;
    for (i = 0; i < max - 1 && src[i]; i++)
        dst[i] = src[i];
    dst[i] = '\0';
}

static int is_name_char(char c)
{
    return (c == '_') ||
           (c >= 'A' && c <= 'Z') ||
           (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9');
}

static int is_name_start(char c)
{
    return (c == '_') ||
           (c >= 'A' && c <= 'Z') ||
           (c >= 'a' && c <= 'z');
}

static void int_to_str(int val, char *buf)
{
    char tmp[16];
    int len;
    int i;
    int neg;

    neg = 0;
    if (val < 0) { neg = 1; val = -val; }
    len = 0;
    if (val == 0)
    {
        tmp[len++] = '0';
    }
    else
    {
        while (val > 0)
        {
            tmp[len++] = '0' + (val % 10);
            val = val / 10;
        }
    }
    i = 0;
    if (neg) buf[i++] = '-';
    while (len > 0) buf[i++] = tmp[--len];
    buf[i] = '\0';
}

/* ================================================================
 *                     VARIABLE TABLE
 * ================================================================ */

static sh_var_t *var_find(const char *name)
{
    int i;
    for (i = 0; i < VAR_MAX; i++)
    {
        if (var_table[i].in_use && sh_streq(var_table[i].name, name))
            return &var_table[i];
    }
    return (sh_var_t *)0;
}

static sh_var_t *var_alloc(void)
{
    int i;
    for (i = 0; i < VAR_MAX; i++)
    {
        if (!var_table[i].in_use)
            return &var_table[i];
    }
    return (sh_var_t *)0;
}

static const char *var_get(const char *name)
{
    sh_var_t *v;
    v = var_find(name);
    return v ? v->value : (const char *)0;
}

static int var_set(const char *name, const char *value)
{
    sh_var_t *v;
    if (!name || !*name) return -1;
    if (sh_strlen(name) >= VAR_NAME_LEN) return -1;
    if (sh_strlen(value) >= VAR_VALUE_LEN) return -1;

    v = var_find(name);
    if (!v)
    {
        v = var_alloc();
        if (!v) return -1;
        v->in_use = 1;
        v->exported = 0;
        sh_strncpy(v->name, name, VAR_NAME_LEN);
    }
    sh_strncpy(v->value, value, VAR_VALUE_LEN);
    return 0;
}

static int var_export(const char *name)
{
    sh_var_t *v;
    v = var_find(name);
    if (!v)
    {
        if (var_set(name, "") < 0) return -1;
        v = var_find(name);
        if (!v) return -1;
    }
    v->exported = 1;
    return 0;
}

static int var_unset(const char *name)
{
    sh_var_t *v;
    v = var_find(name);
    if (!v) return -1;
    v->in_use = 0;
    return 0;
}

static void vars_init(void)
{
    int i;
    for (i = 0; i < VAR_MAX; i++)
        var_table[i].in_use = 0;

    var_set("PATH", "/bin");
    var_export("PATH");
    var_set("HOME", "/");
    var_export("HOME");
}

/* ================================================================
 *                     VARIABLE EXPANSION
 * ================================================================ */

static int expand_append(char *dst, int *off, int max, const char *s)
{
    while (*s)
    {
        if (*off >= max - 1) return -1;
        dst[(*off)++] = *s++;
    }
    return 0;
}

/*
 * Expand $-variables and handle quoting in src → dst.
 * Single quotes: verbatim, no expansion.
 * Double quotes: $-expansion, \" \\ \$ recognised.
 * Bare text: $-expansion, backslash escapes.
 */
static int sh_expand(const char *src, char *dst, int dst_size)
{
    int  off;
    int  in_sq;
    int  in_dq;
    char num_buf[16];

    off = 0;
    in_sq = 0;
    in_dq = 0;

    while (*src)
    {
        char c;
        c = *src;

        /* Inside single quotes: everything literal */
        if (in_sq)
        {
            if (c == '\'') { in_sq = 0; src++; continue; }
            if (off >= dst_size - 1) return -1;
            dst[off++] = c;
            src++;
            continue;
        }

        /* Toggle quotes */
        if (c == '\'' && !in_dq) { in_sq = 1; src++; continue; }
        if (c == '"') { in_dq = !in_dq; src++; continue; }

        /* Backslash escape */
        if (c == '\\' && src[1])
        {
            if (off >= dst_size - 1) return -1;
            dst[off++] = src[1];
            src += 2;
            continue;
        }

        /* $-expansion (inside double quotes or bare text) */
        if (c == '$' && src[1])
        {
            const char *val;
            char name_buf[VAR_NAME_LEN];
            int  nlen;

            val = (const char *)0;
            nlen = 0;
            src++;

            if (*src == '{')
            {
                /* ${NAME} */
                src++;
                while (*src && *src != '}' && nlen < VAR_NAME_LEN - 1)
                    name_buf[nlen++] = *src++;
                name_buf[nlen] = '\0';
                if (*src == '}') src++;
                val = var_get(name_buf);
            }
            else if (*src == '?')
            {
                int_to_str(last_exit, num_buf);
                val = num_buf;
                src++;
            }
            else if (*src == '$')
            {
                int_to_str(sys_getpid(), num_buf);
                val = num_buf;
                src++;
            }
            else if (*src == '#')
            {
                int_to_str(pos_param_count, num_buf);
                val = num_buf;
                src++;
            }
            else if (*src >= '0' && *src <= '9')
            {
                int idx;
                idx = *src - '0';
                val = pos_param_store[idx];
                src++;
            }
            else if (is_name_start(*src))
            {
                while (is_name_char(*src) && nlen < VAR_NAME_LEN - 1)
                    name_buf[nlen++] = *src++;
                name_buf[nlen] = '\0';
                val = var_get(name_buf);
            }
            else
            {
                /* Lone $ — emit literal */
                if (off >= dst_size - 1) return -1;
                dst[off++] = '$';
                continue;
            }

            if (val)
            {
                if (expand_append(dst, &off, dst_size, val) < 0) return -1;
            }
            continue;
        }

        if (off >= dst_size - 1) return -1;
        dst[off++] = c;
        src++;
    }

    if (in_sq || in_dq) return -1;   /* unterminated quote */
    dst[off] = '\0';
    return 0;
}

/* ================================================================
 *                     TOKENIZER
 * ================================================================ */

static int store_putc(char *store, int *off, int store_size, char c)
{
    if (*off >= store_size - 1) return -1;
    store[(*off)++] = c;
    return 0;
}

static int emit_word(sh_tok_t *toks, int *ti, int max,
                     char *store, int *off, int store_size,
                     int word_start)
{
    if (*ti >= max - 1) return -1;
    if (store_putc(store, off, store_size, '\0') < 0) return -1;
    toks[*ti].type = TOK_WORD;
    toks[*ti].text = &store[word_start];
    (*ti)++;
    return 0;
}

static int emit_op(sh_tok_t *toks, int *ti, int max, int type)
{
    if (*ti >= max - 1) return -1;
    toks[*ti].type = type;
    toks[*ti].text = (char *)0;
    (*ti)++;
    return 0;
}

/*
 * Tokenize an already-expanded line into tok_buf.
 * Returns token count or -1 on error.
 * Note: quoting is preserved here for proper word boundaries,
 * but quote characters are consumed (removed from output).
 */
static int sh_lex(const char *line, sh_tok_t *out, int max_toks,
                  char *store, int store_size)
{
    int ti;
    int off;
    int in_word;
    int word_start;
    int in_sq;
    int in_dq;
    const char *p;

    ti = 0;
    off = 0;
    in_word = 0;
    word_start = 0;
    in_sq = 0;
    in_dq = 0;
    p = line;

    while (*p)
    {
        char c;
        c = *p;

        if (in_sq)
        {
            if (c == '\'') { in_sq = 0; p++; continue; }
            if (store_putc(store, &off, store_size, c) < 0) return -1;
            p++;
            continue;
        }

        if (in_dq)
        {
            if (c == '"') { in_dq = 0; p++; continue; }
            if (c == '\\' && p[1])
            {
                if (store_putc(store, &off, store_size, p[1]) < 0) return -1;
                p += 2;
                continue;
            }
            if (store_putc(store, &off, store_size, c) < 0) return -1;
            p++;
            continue;
        }

        /* Outside quotes */

        if (c == ' ' || c == '\t')
        {
            if (in_word)
            {
                if (emit_word(out, &ti, max_toks, store, &off, store_size,
                              word_start) < 0) return -1;
                in_word = 0;
            }
            p++;
            continue;
        }

        if (c == '#' && !in_word)
        {
            /* Comment — rest of line ignored */
            break;
        }

        if (c == '\'' || c == '"')
        {
            if (!in_word) { in_word = 1; word_start = off; }
            if (c == '\'') in_sq = 1; else in_dq = 1;
            p++;
            continue;
        }

        if (c == '\\' && p[1])
        {
            if (!in_word) { in_word = 1; word_start = off; }
            if (store_putc(store, &off, store_size, p[1]) < 0) return -1;
            p += 2;
            continue;
        }

        /* Operator characters break current word */
        if (c == '<' || c == '>' || c == '|' || c == '&' || c == ';')
        {
            if (in_word)
            {
                if (emit_word(out, &ti, max_toks, store, &off, store_size,
                              word_start) < 0) return -1;
                in_word = 0;
            }
            if (c == '>' && p[1] == '>')
            {
                if (emit_op(out, &ti, max_toks, TOK_REDIR_APPEND) < 0)
                    return -1;
                p += 2;
            }
            else if (c == '|' && p[1] == '|')
            {
                if (emit_op(out, &ti, max_toks, TOK_OR) < 0) return -1;
                p += 2;
            }
            else if (c == '&' && p[1] == '&')
            {
                if (emit_op(out, &ti, max_toks, TOK_AND) < 0) return -1;
                p += 2;
            }
            else if (c == '<')
            {
                if (emit_op(out, &ti, max_toks, TOK_REDIR_IN) < 0) return -1;
                p++;
            }
            else if (c == '>')
            {
                if (emit_op(out, &ti, max_toks, TOK_REDIR_OUT) < 0) return -1;
                p++;
            }
            else if (c == '|')
            {
                if (emit_op(out, &ti, max_toks, TOK_PIPE) < 0) return -1;
                p++;
            }
            else if (c == ';')
            {
                if (emit_op(out, &ti, max_toks, TOK_SEMI) < 0) return -1;
                p++;
            }
            else
            {
                /* lone & — not supported */
                return -1;
            }
            continue;
        }

        /* Regular character */
        if (!in_word) { in_word = 1; word_start = off; }
        if (store_putc(store, &off, store_size, c) < 0) return -1;
        p++;
    }

    if (in_sq || in_dq) return -1;

    if (in_word)
    {
        if (emit_word(out, &ti, max_toks, store, &off, store_size,
                      word_start) < 0) return -1;
    }

    out[ti].type = TOK_END;
    out[ti].text = (char *)0;
    return ti;
}

/* ================================================================
 *                     PARSER
 * ================================================================ */

static void parse_error(const char *msg)
{
    sys_putstr("sh: parse error: ");
    sys_putstr(msg);
    sys_putc('\n');
}

static int parse_command(sh_tok_t *toks, int *i, sh_cmd_t *cmd)
{
    int t;
    cmd->argc = 0;
    cmd->redir_in = (char *)0;
    cmd->redir_out = (char *)0;
    cmd->redir_append = (char *)0;

    while (1)
    {
        t = toks[*i].type;
        if (t == TOK_END || t == TOK_PIPE ||
            t == TOK_AND || t == TOK_OR || t == TOK_SEMI)
            break;

        if (t == TOK_WORD)
        {
            if (cmd->argc >= ARGV_MAX)
            {
                parse_error("too many arguments");
                return -1;
            }
            cmd->argv[cmd->argc++] = toks[(*i)++].text;
            continue;
        }

        if (t == TOK_REDIR_IN || t == TOK_REDIR_OUT || t == TOK_REDIR_APPEND)
        {
            int op;
            op = t;
            (*i)++;
            if (toks[*i].type != TOK_WORD)
            {
                parse_error("missing filename after redirect");
                return -1;
            }
            if (op == TOK_REDIR_IN)
                cmd->redir_in = toks[*i].text;
            else if (op == TOK_REDIR_OUT)
                cmd->redir_out = toks[*i].text;
            else
                cmd->redir_append = toks[*i].text;
            (*i)++;
            continue;
        }

        parse_error("unexpected token");
        return -1;
    }

    if (cmd->argc == 0)
    {
        /* Empty command — not always an error (trailing ;) */
        return 1;
    }
    if (cmd->argc < ARGV_MAX)
        cmd->argv[cmd->argc] = (char *)0;
    return 0;
}

static int parse_pipeline(sh_tok_t *toks, int *i, sh_pipeline_t *pl)
{
    int rc;
    pl->n_cmds = 0;
    while (1)
    {
        if (pl->n_cmds >= PIPE_MAX)
        {
            parse_error("pipeline too long");
            return -1;
        }
        rc = parse_command(toks, i, &pl->cmds[pl->n_cmds]);
        if (rc < 0) return -1;
        if (rc == 1) break;  /* empty command */
        pl->n_cmds++;
        if (toks[*i].type != TOK_PIPE) break;
        (*i)++;
    }
    return 0;
}

static int sh_parse(sh_tok_t *toks, sh_chain_t *out)
{
    int i;
    int op;

    i = 0;
    out->n_pipes = 0;

    if (toks[0].type == TOK_END) return 0;

    while (1)
    {
        if (out->n_pipes >= CHAIN_MAX)
        {
            parse_error("chain too long");
            return -1;
        }
        if (parse_pipeline(toks, &i, &out->pipes[out->n_pipes]) < 0)
            return -1;

        /* Skip empty pipelines (e.g. from trailing ;) */
        if (out->pipes[out->n_pipes].n_cmds == 0)
        {
            if (toks[i].type == TOK_END) break;
            if (toks[i].type == TOK_SEMI) { i++; continue; }
            break;
        }

        op = toks[i].type;
        if (op == TOK_AND)       { out->ops[out->n_pipes++] = OP_AND;  i++; }
        else if (op == TOK_OR)   { out->ops[out->n_pipes++] = OP_OR;   i++; }
        else if (op == TOK_SEMI) { out->ops[out->n_pipes++] = OP_SEMI; i++; }
        else if (op == TOK_END)  { out->ops[out->n_pipes++] = OP_END;  break; }
        else { parse_error("expected operator"); return -1; }

        if (toks[i].type == TOK_END)
        {
            /* Trailing ; is allowed; trailing && / || is not */
            if (op == TOK_AND || op == TOK_OR)
            {
                parse_error("trailing && / ||");
                return -1;
            }
            break;
        }
    }
    return 0;
}

/* ================================================================
 *                     OUTPUT HELPERS
 * ================================================================ */

static void print_int(int val)
{
    char buf[16];
    int_to_str(val, buf);
    sys_putstr(buf);
}

/* ================================================================
 *                     PATH RESOLUTION
 * ================================================================ */

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

static int has_external(const char *cmd)
{
    char path[PATH_MAX];
    int ci;

    if (cmd[0] == '/')
        return (sys_stat(cmd, (void *)0) == 0);

    for (ci = 0; cmd[ci]; ci++)
    {
        if (cmd[ci] == '/')
        {
            resolve_path(path, PATH_MAX, cmd);
            return (sys_stat(path, (void *)0) == 0);
        }
    }

    /* Bare command: check /bin/<cmd> */
    path[0] = '/'; path[1] = 'b'; path[2] = 'i';
    path[3] = 'n'; path[4] = '/';
    for (ci = 0; cmd[ci] && ci < PATH_MAX - 6; ci++)
        path[5 + ci] = cmd[ci];
    path[5 + ci] = '\0';

    return (sys_stat(path, (void *)0) == 0);
}

static void resolve_cmd_path(const char *cmd, char *resolved)
{
    int ci;
    int has_slash;

    if (cmd[0] == '/')
    {
        for (ci = 0; cmd[ci] && ci < PATH_MAX - 1; ci++)
            resolved[ci] = cmd[ci];
        resolved[ci] = '\0';
        return;
    }

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

/* ================================================================
 *                     BUILT-IN COMMANDS
 * ================================================================ */

static int bi_help(int argc, char **argv)
{
    (void)argc; (void)argv;
    sys_putstr("Built-in commands:\n");
    sys_putstr("  help              show this message\n");
    sys_putstr("  clear             clear screen\n");
    sys_putstr("  cd [DIR]          change directory\n");
    sys_putstr("  pwd               print working directory\n");
    sys_putstr("  echo [ARGS..]     print arguments\n");
    sys_putstr("  export NAME[=VAL] export variable\n");
    sys_putstr("  set               list variables\n");
    sys_putstr("  unset NAME        remove variable\n");
    sys_putstr("  env               list exported vars\n");
    sys_putstr("  test EXPR / [ EXPR ]  evaluate condition\n");
    sys_putstr("  true / false      return 0 / 1\n");
    sys_putstr("  history           show command history\n");
    sys_putstr("  exit [N]          exit shell\n");
    sys_putstr("  halt              stop the system\n");
    sys_putstr("External programs in /bin/:\n");
    sys_putstr("  ls cat cp mv rm mkdir touch\n");
    sys_putstr("  ps free df kill sync\n");
    sys_putstr("  grep head wc tree\n");
    return 0;
}

static int bi_clear(int argc, char **argv)
{
    (void)argc; (void)argv;
    sys_putstr("\x1b[2J\x1b[H");
    return 0;
}

static int bi_echo(int argc, char **argv)
{
    int i;
    for (i = 1; i < argc; i++)
    {
        if (i > 1) sys_putc(' ');
        sys_putstr(argv[i]);
    }
    sys_putc('\n');
    return 0;
}

static int bi_pwd(int argc, char **argv)
{
    (void)argc; (void)argv;
    sys_putstr(cwd);
    sys_putc('\n');
    return 0;
}

static int bi_cd(int argc, char **argv)
{
    char resolved[CWD_MAX];
    char *target;

    if (argc < 2 || !argv[1][0])
    {
        sys_chdir("/");
        sys_getcwd(cwd, CWD_MAX);
        return 0;
    }

    target = argv[1];

    /* Handle ".." */
    if (target[0] == '.' && target[1] == '.' && target[2] == '\0')
    {
        int len;
        len = sh_strlen(cwd);
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
    resolve_path(resolved, CWD_MAX, target);

    {
        char entry_buf[64];
        if (sys_readdir(resolved, entry_buf, 1) < 0)
        {
            sys_putstr("cd: no such directory: ");
            sys_putstr(target);
            sys_putc('\n');
            return 1;
        }
    }

    sh_strncpy(cwd, resolved, CWD_MAX);
    sys_chdir(cwd);
    return 0;
}

static int bi_export(int argc, char **argv)
{
    int i;

    if (argc < 2)
    {
        /* List exported variables */
        for (i = 0; i < VAR_MAX; i++)
        {
            if (var_table[i].in_use && var_table[i].exported)
            {
                sys_putstr("export ");
                sys_putstr(var_table[i].name);
                sys_putc('=');
                sys_putstr(var_table[i].value);
                sys_putc('\n');
            }
        }
        return 0;
    }

    for (i = 1; i < argc; i++)
    {
        char *arg;
        char *eq;
        int j;

        arg = argv[i];
        eq = (char *)0;
        for (j = 0; arg[j]; j++)
        {
            if (arg[j] == '=') { eq = &arg[j]; break; }
        }

        if (eq)
        {
            *eq = '\0';
            var_set(arg, eq + 1);
            var_export(arg);
            *eq = '=';
        }
        else
        {
            var_export(arg);
        }
    }
    return 0;
}

static int bi_set(int argc, char **argv)
{
    int i;
    (void)argc; (void)argv;
    for (i = 0; i < VAR_MAX; i++)
    {
        if (var_table[i].in_use)
        {
            sys_putstr(var_table[i].name);
            sys_putc('=');
            sys_putstr(var_table[i].value);
            sys_putc('\n');
        }
    }
    return 0;
}

static int bi_unset(int argc, char **argv)
{
    int i;
    for (i = 1; i < argc; i++)
        var_unset(argv[i]);
    return 0;
}

static int bi_env(int argc, char **argv)
{
    int i;
    (void)argc; (void)argv;
    for (i = 0; i < VAR_MAX; i++)
    {
        if (var_table[i].in_use && var_table[i].exported)
        {
            sys_putstr(var_table[i].name);
            sys_putc('=');
            sys_putstr(var_table[i].value);
            sys_putc('\n');
        }
    }
    return 0;
}

/*
 * test / [ — Evaluate simple conditional expressions.
 * Supports:
 *   test STRING           (true if non-empty)
 *   test -n STRING        (true if non-empty)
 *   test -z STRING        (true if empty)
 *   test S1 = S2          (string equality)
 *   test S1 != S2         (string inequality)
 *   test N1 -eq N2        (integer equality)
 *   test N1 -ne N2        (integer inequality)
 *   test N1 -lt N2        (integer less-than)
 *   test N1 -gt N2        (integer greater-than)
 *   test -f PATH          (file exists)
 *   test -d PATH          (directory exists — uses readdir)
 *   test ! EXPR           (logical NOT)
 */
static int parse_int(const char *s)
{
    int val;
    int neg;

    val = 0;
    neg = 0;
    if (*s == '-') { neg = 1; s++; }
    while (*s >= '0' && *s <= '9')
    {
        val = val * 10 + (*s - '0');
        s++;
    }
    return neg ? -val : val;
}

static int test_eval(int argc, char **argv, int start)
{
    char *a;

    if (start >= argc) return 1; /* no args → false */

    a = argv[start];

    /* Unary ! */
    if (sh_streq(a, "!") && start + 1 < argc)
        return !test_eval(argc, argv, start + 1) ? 0 : 1;

    /* Unary -n / -z / -f / -d */
    if (a[0] == '-' && a[2] == '\0' && start + 1 < argc)
    {
        char *operand;
        operand = argv[start + 1];

        if (a[1] == 'n') return operand[0] ? 0 : 1;
        if (a[1] == 'z') return operand[0] ? 1 : 0;
        if (a[1] == 'f') return (sys_stat(operand, (void *)0) == 0) ? 0 : 1;
        if (a[1] == 'd')
        {
            char tmp[64];
            return (sys_readdir(operand, tmp, 1) >= 0) ? 0 : 1;
        }
    }

    /* Binary operators: S1 = S2, S1 != S2, N1 -eq/-ne/-lt/-gt N2 */
    if (start + 2 < argc)
    {
        char *op;
        char *right;

        op = argv[start + 1];
        right = argv[start + 2];

        if (sh_streq(op, "="))
            return sh_streq(a, right) ? 0 : 1;
        if (sh_streq(op, "!="))
            return sh_streq(a, right) ? 1 : 0;
        if (sh_streq(op, "-eq"))
            return (parse_int(a) == parse_int(right)) ? 0 : 1;
        if (sh_streq(op, "-ne"))
            return (parse_int(a) != parse_int(right)) ? 0 : 1;
        if (sh_streq(op, "-lt"))
            return (parse_int(a) < parse_int(right)) ? 0 : 1;
        if (sh_streq(op, "-gt"))
            return (parse_int(a) > parse_int(right)) ? 0 : 1;
    }

    /* Unary: test STRING (true if non-empty) */
    return a[0] ? 0 : 1;
}

static int bi_test(int argc, char **argv)
{
    /* If invoked as '[', strip trailing ']' */
    if (sh_streq(argv[0], "["))
    {
        if (argc < 2 || !sh_streq(argv[argc - 1], "]"))
        {
            sys_putstr("sh: [: missing ]\n");
            return 2;
        }
        argc--;
    }
    return test_eval(argc, argv, 1);
}

static int bi_true(int argc, char **argv)
{
    (void)argc; (void)argv;
    return 0;
}

static int bi_false(int argc, char **argv)
{
    (void)argc; (void)argv;
    return 1;
}

/* ---- Builtin dispatch table ---- */

typedef int (*builtin_fn)(int argc, char **argv);

struct builtin_entry {
    const char *name;
    builtin_fn  fn;
};

/* Forward declaration (defined in COMMAND HISTORY section below) */
static int bi_history(int argc, char **argv);

static struct builtin_entry builtins[] = {
    { "help",    bi_help },
    { "clear",   bi_clear },
    { "echo",    bi_echo },
    { "cd",      bi_cd },
    { "pwd",     bi_pwd },
    { "export",  bi_export },
    { "set",     bi_set },
    { "unset",   bi_unset },
    { "env",     bi_env },
    { "test",    bi_test },
    { "[",       bi_test },
    { "true",    bi_true },
    { "false",   bi_false },
    { "history", bi_history },
    { (const char *)0, (builtin_fn)0 }
};

static builtin_fn find_builtin(const char *name)
{
    int i;
    for (i = 0; builtins[i].name; i++)
    {
        if (sh_streq(builtins[i].name, name))
            return builtins[i].fn;
    }
    return (builtin_fn)0;
}

/* ================================================================
 *                     EXTERNAL COMMAND EXECUTION
 * ================================================================ */

static int run_external(sh_cmd_t *cmd)
{
    char resolved[PATH_MAX];
    int pid;
    int exit_code;

    resolve_cmd_path(cmd->argv[0], resolved);

    pid = sys_spawn(resolved, cmd->argc, (const char **)cmd->argv);
    if (pid < 0)
    {
        sys_putstr(cmd->argv[0]);
        sys_putstr(": command not found\n");
        return 127;
    }

    exit_code = sys_waitpid(pid);
    return exit_code;
}

/* ================================================================
 *                     PIPE + REDIRECT HELPERS
 * ================================================================ */

static void pipe_path(char *buf, int idx)
{
    buf[0] = '/'; buf[1] = 't'; buf[2] = 'm'; buf[3] = 'p';
    buf[4] = '/'; buf[5] = 'p'; buf[6] = '.';
    buf[7] = '0' + (char)idx;
    buf[8] = '\0';
}

/* Apply redirections for a single command. Returns 0 on success. */
static int apply_redirects(sh_cmd_t *cmd, int pipe_in_fd, int pipe_out_fd)
{
    char rpath[CWD_MAX];
    int fd;

    /* Pipe input (from previous stage) */
    if (pipe_in_fd > 0)
    {
        sys_dup2(0, SAVE_FD_IN);
        sys_dup2(pipe_in_fd, 0);
        sys_close(pipe_in_fd);
    }

    /* Input redirect (< file) overrides pipe input */
    if (cmd->redir_in)
    {
        resolve_path(rpath, CWD_MAX, cmd->redir_in);
        fd = sys_open(rpath, O_RDONLY);
        if (fd < 0)
        {
            sys_putstr("sh: cannot open ");
            sys_putstr(cmd->redir_in);
            sys_putc('\n');
            return -1;
        }
        if (!pipe_in_fd) sys_dup2(0, SAVE_FD_IN);
        sys_dup2(fd, 0);
        sys_close(fd);
    }

    /* Pipe output (to next stage) */
    if (pipe_out_fd > 0)
    {
        sys_dup2(1, SAVE_FD);
        sys_dup2(pipe_out_fd, 1);
        sys_close(pipe_out_fd);
    }

    /* Output redirect (> file or >> file) overrides pipe output */
    if (cmd->redir_out)
    {
        resolve_path(rpath, CWD_MAX, cmd->redir_out);
        fd = sys_open(rpath, O_WRONLY | O_CREAT);
        if (fd < 0)
        {
            sys_putstr("sh: cannot open ");
            sys_putstr(cmd->redir_out);
            sys_putc('\n');
            return -1;
        }
        if (!pipe_out_fd) sys_dup2(1, SAVE_FD);
        sys_dup2(fd, 1);
        sys_close(fd);
    }
    else if (cmd->redir_append)
    {
        resolve_path(rpath, CWD_MAX, cmd->redir_append);
        fd = sys_open(rpath, O_WRONLY | O_CREAT | O_APPEND);
        if (fd < 0)
        {
            sys_putstr("sh: cannot open ");
            sys_putstr(cmd->redir_append);
            sys_putc('\n');
            return -1;
        }
        if (!pipe_out_fd) sys_dup2(1, SAVE_FD);
        sys_dup2(fd, 1);
        sys_close(fd);
    }

    return 0;
}

/* Restore fds after redirects */
static void restore_fds(sh_cmd_t *cmd, int had_pipe_in, int had_pipe_out)
{
    if (had_pipe_out || cmd->redir_out || cmd->redir_append)
    {
        sys_dup2(SAVE_FD, 1);
        sys_close(SAVE_FD);
    }
    if (had_pipe_in || cmd->redir_in)
    {
        sys_dup2(SAVE_FD_IN, 0);
        sys_close(SAVE_FD_IN);
    }
}

/* ================================================================
 *                     COMMAND EXECUTION
 * ================================================================ */

/* Check if a line is a variable assignment (NAME=value, no command) */
static int try_var_assign(sh_cmd_t *cmd)
{
    char *s;
    char *eq;
    int i;
    char name[VAR_NAME_LEN];
    int nlen;

    if (cmd->argc != 1) return 0;

    s = cmd->argv[0];
    eq = (char *)0;
    for (i = 0; s[i]; i++)
    {
        if (s[i] == '=') { eq = &s[i]; break; }
    }
    if (!eq || eq == s) return 0;

    /* Validate name */
    if (!is_name_start(s[0])) return 0;
    nlen = 0;
    for (i = 0; &s[i] < eq; i++)
    {
        if (!is_name_char(s[i])) return 0;
        if (nlen < VAR_NAME_LEN - 1) name[nlen++] = s[i];
    }
    name[nlen] = '\0';

    var_set(name, eq + 1);
    return 1;
}

/* ---- Script file execution ---- */

/* Forward declaration — defined later in the file */
static int execute_lines(const char **lines, int nlines);

/* Check if a file starts with #! (shebang). */
static int is_script_file(const char *path)
{
    int fd;
    char hdr[2];
    int n;

    fd = sys_open(path, O_RDONLY);
    if (fd < 0) return 0;

    n = sys_read(fd, hdr, 2);
    sys_close(fd);

    return (n == 2 && hdr[0] == '#' && hdr[1] == '!');
}

/*
 * Execute a shell script file.
 * Reads the entire file, splits into lines, skips the shebang,
 * sets positional parameters ($0-$9, $#), then runs via execute_lines().
 * Aborts on the first command that returns non-zero (implicit set -e).
 */
static char  file_buf[SCRIPT_FILE_MAX];

static int run_script(const char *path, int argc, char **argv)
{
    int fd;
    int file_size;
    int n;
    int i;
    int line_count;
    int result;
    char *lines[SCRIPT_LINES_MAX];

    if (in_script)
    {
        sys_putstr("sh: nested scripts not supported\n");
        return 1;
    }

    /* Open and read file */
    fd = sys_open(path, O_RDONLY);
    if (fd < 0)
    {
        sys_putstr("sh: cannot open ");
        sys_putstr(path);
        sys_putc('\n');
        return 1;
    }

    file_size = sys_lseek(fd, 0, SEEK_END);
    sys_lseek(fd, 0, SEEK_SET);

    if (file_size <= 0 || file_size >= SCRIPT_FILE_MAX - 1)
    {
        sys_close(fd);
        sys_putstr("sh: script too large\n");
        return 1;
    }

    n = sys_read(fd, file_buf, file_size);
    sys_close(fd);
    if (n <= 0) return 1;
    file_buf[n] = '\0';

    /* Split into lines, skip shebang line */
    line_count = 0;
    i = 0;

    /* Skip first line (shebang) */
    while (i < n && file_buf[i] != '\n') i++;
    if (i < n) i++;

    while (i < n && line_count < SCRIPT_LINES_MAX)
    {
        lines[line_count++] = &file_buf[i];
        while (i < n && file_buf[i] != '\n') i++;
        if (i < n) file_buf[i++] = '\0';
    }

    /* Set positional parameters */
    pos_param_count = (argc > 1) ? argc - 1 : 0;
    sh_strncpy(pos_param_store[0], path, PATH_MAX);
    for (i = 1; i < argc && i < 10; i++)
        sh_strncpy(pos_param_store[i], argv[i], PATH_MAX);
    for (; i < 10; i++)
        pos_param_store[i][0] = '\0';

    /* Execute all lines */
    in_script = 1;
    result = execute_lines((const char **)lines, line_count);
    in_script = 0;

    /* Clear positional parameters */
    pos_param_count = 0;
    for (i = 0; i < 10; i++)
        pos_param_store[i][0] = '\0';

    return result;
}

/* Run a single command (builtin or external). */
static int run_cmd(sh_cmd_t *cmd)
{
    builtin_fn fn;

    /* Variable assignment? */
    if (try_var_assign(cmd))
        return 0;

    /* exit with optional code */
    if (sh_streq(cmd->argv[0], "exit"))
    {
        int code;
        code = 0;
        if (cmd->argc >= 2)
            code = parse_int(cmd->argv[1]);
        sys_exit(code);
        return 0; /* not reached */
    }

    /* halt */
    if (sh_streq(cmd->argv[0], "halt"))
    {
        sys_putstr("System halted.\n");
        sys_exit(0);
        return 0; /* not reached */
    }

    /* Builtin? */
    fn = find_builtin(cmd->argv[0]);
    if (fn)
        return fn(cmd->argc, cmd->argv);

    /* External */
    if (has_external(cmd->argv[0]))
    {
        char resolved[PATH_MAX];
        resolve_cmd_path(cmd->argv[0], resolved);

        /* Script file? (starts with #!) */
        if (is_script_file(resolved))
            return run_script(resolved, cmd->argc, cmd->argv);

        return run_external(cmd);
    }

    sys_putstr(cmd->argv[0]);
    sys_putstr(": command not found\n");
    return 127;
}

/* Run a pipeline (single command or multi-stage pipe via temp files). */
static int run_pipeline(sh_pipeline_t *pl)
{
    int result;
    int i;
    char pp[16];

    if (pl->n_cmds == 0) return 0;

    /* Single command — no temp files needed */
    if (pl->n_cmds == 1)
    {
        sh_cmd_t *cmd;
        cmd = &pl->cmds[0];

        if (apply_redirects(cmd, 0, 0) < 0) return 1;
        result = run_cmd(cmd);
        restore_fds(cmd, 0, 0);
        return result;
    }

    /* Multi-stage pipeline via temp files */
    result = 0;
    for (i = 0; i < pl->n_cmds; i++)
    {
        sh_cmd_t *cmd;
        int pipe_out_fd;
        int pipe_in_fd;
        int is_last;

        cmd = &pl->cmds[i];
        pipe_out_fd = 0;
        pipe_in_fd = 0;
        is_last = (i == pl->n_cmds - 1);

        /* Open output temp file (except last) */
        if (!is_last)
        {
            pipe_path(pp, i);
            pipe_out_fd = sys_open(pp, O_WRONLY | O_CREAT);
            if (pipe_out_fd < 0)
            {
                sys_putstr("sh: cannot create pipe\n");
                result = 1;
                goto pipe_done;
            }
        }

        /* Open input from previous stage (except first) */
        if (i > 0)
        {
            pipe_path(pp, i - 1);
            pipe_in_fd = sys_open(pp, O_RDONLY);
            if (pipe_in_fd < 0)
            {
                sys_putstr("sh: cannot open pipe\n");
                if (pipe_out_fd > 0) sys_close(pipe_out_fd);
                result = 1;
                goto pipe_done;
            }
        }

        if (apply_redirects(cmd, pipe_in_fd, pipe_out_fd) < 0)
        {
            result = 1;
            goto pipe_done;
        }

        result = run_cmd(cmd);
        restore_fds(cmd, (i > 0), !is_last);
    }

pipe_done:
    /* Clean up temp files */
    for (i = 0; i < pl->n_cmds - 1; i++)
    {
        pipe_path(pp, i);
        sys_unlink(pp);
    }
    return result;
}

/* Run a chain of pipelines connected by &&, ||, ; */
static void run_chain(sh_chain_t *chain)
{
    int i;
    int result;

    result = 0;
    for (i = 0; i < chain->n_pipes; i++)
    {
        int op;

        /* Check short-circuit */
        if (i > 0)
        {
            op = chain->ops[i - 1];
            if (op == OP_AND && result != 0) continue;
            if (op == OP_OR && result == 0) continue;
        }

        result = run_pipeline(&chain->pipes[i]);
        last_exit = result;
    }
}

/* ================================================================
 *                     GLOB EXPANSION
 * ================================================================ */

#define GLOB_DIR_ENTRIES 64
#define GLOB_DIR_WORDS   8
#define GLOB_FNAME_WORDS 4
#define GLOB_NAME_LEN    17

static void glob_decompress(char *dest, unsigned int *src)
{
    int wi;
    unsigned int word;
    unsigned int c;
    int ci;

    ci = 0;
    for (wi = 0; wi < GLOB_FNAME_WORDS; wi++)
    {
        word = src[wi];
        c = (word >> 24) & 0xFF;
        dest[ci++] = (char)c;
        if (c == 0) return;
        c = (word >> 16) & 0xFF;
        dest[ci++] = (char)c;
        if (c == 0) return;
        c = (word >> 8) & 0xFF;
        dest[ci++] = (char)c;
        if (c == 0) return;
        c = word & 0xFF;
        dest[ci++] = (char)c;
        if (c == 0) return;
    }
    dest[ci] = '\0';
}

/* Check if a string contains glob characters */
static int has_glob(const char *s)
{
    while (*s)
    {
        if (*s == '*' || *s == '?' || *s == '[') return 1;
        s++;
    }
    return 0;
}

/* Match a pattern against a string (supports *, ?, [abc], [a-z]) */
static int glob_match(const char *pat, const char *str)
{
    while (*pat)
    {
        if (*pat == '*')
        {
            pat++;
            /* Skip consecutive stars */
            while (*pat == '*') pat++;
            if (*pat == '\0') return 1;
            while (*str)
            {
                if (glob_match(pat, str)) return 1;
                str++;
            }
            return 0;
        }
        if (*pat == '?')
        {
            if (*str == '\0') return 0;
            pat++;
            str++;
            continue;
        }
        if (*pat == '[')
        {
            int inv;
            int matched;
            pat++;
            inv = 0;
            if (*pat == '!' || *pat == '^') { inv = 1; pat++; }
            matched = 0;
            while (*pat && *pat != ']')
            {
                if (pat[1] == '-' && pat[2] && pat[2] != ']')
                {
                    if (*str >= pat[0] && *str <= pat[2]) matched = 1;
                    pat += 3;
                }
                else
                {
                    if (*str == *pat) matched = 1;
                    pat++;
                }
            }
            if (*pat == ']') pat++;
            if (inv) matched = !matched;
            if (!matched || *str == '\0') return 0;
            str++;
            continue;
        }
        if (*pat != *str) return 0;
        pat++;
        str++;
    }
    return (*str == '\0');
}

/*
 * Expand glob patterns in a token array.
 * Modifies toks/ntoks in place, using out_store for new word storage.
 * Returns new token count, or -1 on error.
 */
static int glob_expand(sh_tok_t *toks, int ntoks, int max_toks,
                       char *out_store, int store_size)
{
    sh_tok_t new_toks[TOK_MAX];
    int new_count;
    int soff;
    int i;

    new_count = 0;
    soff = 0;

    for (i = 0; i <= ntoks; i++)
    {
        if (i == ntoks || toks[i].type == TOK_END)
        {
            if (new_count >= max_toks) return -1;
            new_toks[new_count].type = TOK_END;
            new_toks[new_count].text = (char *)0;
            break;
        }

        if (toks[i].type != TOK_WORD || !has_glob(toks[i].text))
        {
            if (new_count >= max_toks - 1) return -1;
            new_toks[new_count++] = toks[i];
            continue;
        }

        /* This word has a glob pattern — expand it */
        {
            char *pattern;
            char dir_path[CWD_MAX];
            char file_pat[GLOB_NAME_LEN];
            int last_slash;
            int j;
            unsigned int entry_buf[GLOB_DIR_ENTRIES * GLOB_DIR_WORDS];
            int count;
            int matched;
            char name[GLOB_NAME_LEN];

            pattern = toks[i].text;
            last_slash = -1;
            for (j = 0; pattern[j]; j++)
            {
                if (pattern[j] == '/') last_slash = j;
            }

            if (last_slash >= 0)
            {
                /* Split into dir + file pattern */
                if (last_slash == 0)
                {
                    dir_path[0] = '/';
                    dir_path[1] = '\0';
                }
                else
                {
                    for (j = 0; j < last_slash && j < CWD_MAX - 1; j++)
                        dir_path[j] = pattern[j];
                    dir_path[j] = '\0';
                }
                sh_strncpy(file_pat, &pattern[last_slash + 1], GLOB_NAME_LEN);
            }
            else
            {
                /* Pattern in current directory */
                sh_strncpy(dir_path, cwd, CWD_MAX);
                sh_strncpy(file_pat, pattern, GLOB_NAME_LEN);
            }

            count = sys_readdir(dir_path, entry_buf, GLOB_DIR_ENTRIES);
            matched = 0;

            for (j = 0; j < count; j++)
            {
                unsigned int *entry;
                int nlen;
                int k;

                entry = entry_buf + (j * GLOB_DIR_WORDS);
                glob_decompress(name, entry);

                /* Skip . and .. */
                if (name[0] == '.' && (name[1] == '\0' ||
                    (name[1] == '.' && name[2] == '\0')))
                    continue;

                /* Skip hidden files unless pattern starts with . */
                if (name[0] == '.' && file_pat[0] != '.') continue;

                if (!glob_match(file_pat, name)) continue;

                /* Add match */
                if (new_count >= max_toks - 1) return -1;
                nlen = sh_strlen(name);

                if (last_slash >= 0)
                {
                    /* Prepend directory path */
                    int dlen;
                    dlen = sh_strlen(dir_path);
                    if (soff + dlen + 1 + nlen + 1 > store_size) return -1;
                    for (k = 0; k < dlen; k++)
                        out_store[soff + k] = dir_path[k];
                    out_store[soff + dlen] = '/';
                    for (k = 0; k <= nlen; k++)
                        out_store[soff + dlen + 1 + k] = name[k];
                    new_toks[new_count].type = TOK_WORD;
                    new_toks[new_count].text = &out_store[soff];
                    new_count++;
                    soff += dlen + 1 + nlen + 1;
                }
                else
                {
                    if (soff + nlen + 1 > store_size) return -1;
                    for (k = 0; k <= nlen; k++)
                        out_store[soff + k] = name[k];
                    new_toks[new_count].type = TOK_WORD;
                    new_toks[new_count].text = &out_store[soff];
                    new_count++;
                    soff += nlen + 1;
                }
                matched++;
            }

            if (matched == 0)
            {
                /* No matches — keep the pattern as-is (like bash) */
                if (new_count >= max_toks - 1) return -1;
                new_toks[new_count++] = toks[i];
            }
        }
    }

    /* Copy back */
    for (i = 0; i <= new_count; i++)
        toks[i] = new_toks[i];

    return new_count;
}

/* ================================================================
 *                     CONTROL FLOW ENGINE
 * ================================================================ */

/*
 * Script buffer for multi-line blocks (if/for/while).
 * Lines are stored contiguously separated by '\0'.
 */
#define SCRIPT_MAX  4096

static char  script_buf[SCRIPT_MAX];
static int   script_len;

/* Forward declarations */
static int execute_line(const char *line);
static int execute_lines(const char **lines, int nlines);

/* Check if first word of a line matches a keyword */
static int first_word_is(const char *line, const char *kw)
{
    while (*line == ' ' || *line == '\t') line++;
    while (*kw)
    {
        if (*line != *kw) return 0;
        line++;
        kw++;
    }
    return (*line == '\0' || *line == ' ' || *line == '\t' ||
            *line == ';' || *line == '\n');
}

/* Extract lines[start..end) from a script lines array */
/* Find matching fi/done for a block starting at 'start'.
 * Returns index of the closing keyword, or -1. */
static int find_block_end(const char **lines, int nlines, int start,
                          const char *open_kw, const char *close_kw)
{
    int depth;
    int i;

    depth = 1;
    for (i = start; i < nlines; i++)
    {
        if (first_word_is(lines[i], open_kw))
            depth++;
        if (first_word_is(lines[i], close_kw))
        {
            depth--;
            if (depth == 0) return i;
        }
    }
    return -1;
}

/*
 * Execute an if/elif/else/fi block.
 * lines[0] = "if COND" or "elif COND"
 * Returns the exit code of the executed branch.
 */
static int exec_if_block(const char **lines, int nlines)
{
    int i;
    int cond_result;
    int in_body;
    int body_start;
    int branch_found;

    /* Phase: scan for if/elif/else/fi at depth 0 */
    i = 0;
    branch_found = 0;

    while (i < nlines)
    {
        /* if / elif line: evaluate condition */
        if ((first_word_is(lines[i], "if") || first_word_is(lines[i], "elif"))
            && !branch_found)
        {
            /* Extract condition: skip "if " or "elif " */
            const char *cond_line;
            cond_line = lines[i];
            while (*cond_line == ' ' || *cond_line == '\t') cond_line++;
            if (first_word_is(lines[i], "elif"))
                cond_line += 4;
            else
                cond_line += 2;
            while (*cond_line == ' ' || *cond_line == '\t') cond_line++;

            cond_result = execute_line(cond_line);
            i++;

            /* Expect "then" */
            if (i < nlines && first_word_is(lines[i], "then"))
                i++;

            /* Collect body until elif/else/fi at depth 0 */
            body_start = i;
            {
                int depth;
                depth = 0;
                while (i < nlines)
                {
                    if (first_word_is(lines[i], "if")) depth++;
                    if (first_word_is(lines[i], "fi"))
                    {
                        if (depth == 0) break;
                        depth--;
                    }
                    if (depth == 0 &&
                        (first_word_is(lines[i], "elif") ||
                         first_word_is(lines[i], "else")))
                        break;
                    i++;
                }
            }

            if (cond_result == 0)
            {
                /* Execute this branch */
                execute_lines(&lines[body_start], i - body_start);
                branch_found = 1;
                /* Skip to fi */
                {
                    int depth;
                    depth = 0;
                    while (i < nlines)
                    {
                        if (first_word_is(lines[i], "if")) depth++;
                        if (first_word_is(lines[i], "fi"))
                        {
                            if (depth == 0) break;
                            depth--;
                        }
                        i++;
                    }
                }
            }
            continue;
        }

        /* else */
        if (first_word_is(lines[i], "else") && !branch_found)
        {
            i++;
            body_start = i;
            while (i < nlines && !first_word_is(lines[i], "fi"))
                i++;
            execute_lines(&lines[body_start], i - body_start);
            branch_found = 1;
            continue;
        }

        /* fi */
        if (first_word_is(lines[i], "fi"))
        {
            i++;
            break;
        }

        i++;
    }

    return last_exit;
}

/*
 * Execute a for/in/do/done block.
 * lines[0] = "for VAR in WORD..."
 */
static int exec_for_block(const char **lines, int nlines)
{
    /* Parse "for VAR in WORD..." from first line */
    const char *p;
    char varname[VAR_NAME_LEN];
    int vlen;
    char words_buf[EXPAND_MAX];
    sh_tok_t words_toks[TOK_MAX];
    char words_store[TOK_STORE_SIZE];
    int nwords;
    int body_start;
    int body_end;
    int w;
    int result;

    p = lines[0];
    while (*p == ' ' || *p == '\t') p++;
    p += 3; /* skip "for" */
    while (*p == ' ' || *p == '\t') p++;

    /* Extract variable name */
    vlen = 0;
    while (is_name_char(*p) && vlen < VAR_NAME_LEN - 1)
        varname[vlen++] = *p++;
    varname[vlen] = '\0';

    if (vlen == 0)
    {
        sys_putstr("sh: for: missing variable\n");
        return 1;
    }

    /* Skip "in" */
    while (*p == ' ' || *p == '\t') p++;
    if (p[0] == 'i' && p[1] == 'n' && (p[2] == ' ' || p[2] == '\t'))
        p += 2;
    else
    {
        sys_putstr("sh: for: missing 'in'\n");
        return 1;
    }

    /* Expand and tokenize the word list */
    if (sh_expand(p, words_buf, EXPAND_MAX) < 0)
    {
        sys_putstr("sh: for: expansion error\n");
        return 1;
    }
    nwords = sh_lex(words_buf, words_toks, TOK_MAX, words_store, TOK_STORE_SIZE);
    if (nwords < 0) return 1;

    /* Find do/done */
    body_start = 1;
    if (body_start < nlines && first_word_is(lines[body_start], "do"))
        body_start++;

    body_end = nlines;
    {
        int i;
        int depth;
        depth = 0;
        for (i = body_start; i < nlines; i++)
        {
            if (first_word_is(lines[i], "for") ||
                first_word_is(lines[i], "while"))
                depth++;
            if (first_word_is(lines[i], "done"))
            {
                if (depth == 0) { body_end = i; break; }
                depth--;
            }
        }
    }

    /* Iterate */
    result = 0;
    for (w = 0; w < nwords; w++)
    {
        if (words_toks[w].type != TOK_WORD) continue;
        var_set(varname, words_toks[w].text);
        result = execute_lines(&lines[body_start], body_end - body_start);
    }

    return result;
}

/*
 * Execute a while/do/done block.
 * lines[0] = "while COND"
 */
static int exec_while_block(const char **lines, int nlines)
{
    const char *cond;
    int body_start;
    int body_end;
    int result;
    int limit;

    cond = lines[0];
    while (*cond == ' ' || *cond == '\t') cond++;
    cond += 5; /* skip "while" */
    while (*cond == ' ' || *cond == '\t') cond++;

    body_start = 1;
    if (body_start < nlines && first_word_is(lines[body_start], "do"))
        body_start++;

    body_end = nlines;
    {
        int i;
        int depth;
        depth = 0;
        for (i = body_start; i < nlines; i++)
        {
            if (first_word_is(lines[i], "for") ||
                first_word_is(lines[i], "while"))
                depth++;
            if (first_word_is(lines[i], "done"))
            {
                if (depth == 0) { body_end = i; break; }
                depth--;
            }
        }
    }

    result = 0;
    limit = 10000; /* safety limit */
    while (limit-- > 0)
    {
        if (execute_line(cond) != 0) break;
        result = execute_lines(&lines[body_start], body_end - body_start);
    }

    return result;
}

/*
 * Execute a sequence of lines (script).
 * Handles if/for/while by collecting sub-blocks.
 */
static int execute_lines(const char **lines, int nlines)
{
    int i;
    int result;

    result = 0;
    i = 0;
    while (i < nlines)
    {
        const char *line;
        line = lines[i];

        /* Skip empty lines */
        {
            const char *p;
            p = line;
            while (*p == ' ' || *p == '\t') p++;
            if (*p == '\0' || *p == '#') { i++; continue; }
        }

        /* if block */
        if (first_word_is(line, "if"))
        {
            int end;
            end = find_block_end(lines, nlines, i + 1, "if", "fi");
            if (end < 0)
            {
                sys_putstr("sh: missing fi\n");
                return 1;
            }
            result = exec_if_block(&lines[i], end - i + 1);
            last_exit = result;
            if (in_script && result != 0) return result;
            i = end + 1;
            continue;
        }

        /* for block */
        if (first_word_is(line, "for"))
        {
            int end;
            end = find_block_end(lines, nlines, i + 1, "for", "done");
            if (end < 0)
            {
                /* Also check "while" as matching open */
                end = find_block_end(lines, nlines, i + 1, "while", "done");
            }
            if (end < 0)
            {
                sys_putstr("sh: missing done\n");
                return 1;
            }
            result = exec_for_block(&lines[i], end - i + 1);
            last_exit = result;
            if (in_script && result != 0) return result;
            i = end + 1;
            continue;
        }

        /* while block */
        if (first_word_is(line, "while"))
        {
            int end;
            end = find_block_end(lines, nlines, i + 1, "while", "done");
            if (end < 0)
            {
                end = find_block_end(lines, nlines, i + 1, "for", "done");
            }
            if (end < 0)
            {
                sys_putstr("sh: missing done\n");
                return 1;
            }
            result = exec_while_block(&lines[i], end - i + 1);
            last_exit = result;
            if (in_script && result != 0) return result;
            i = end + 1;
            continue;
        }

        /* Regular line */
        result = execute_line(line);
        last_exit = result;
        if (in_script && result != 0) return result;
        i++;
    }

    return result;
}

/* ================================================================
 *                     MAIN EXECUTE FUNCTION
 * ================================================================ */

/*
 * Execute a single line (expand → lex → parse → run chain).
 * Returns exit code.
 */
static int execute_line(const char *line)
{
    char exp_buf[EXPAND_MAX];
    sh_tok_t toks[TOK_MAX];
    char tstore[TOK_STORE_SIZE];
    int ntoks;
    sh_chain_t chain;

    /* Skip empty / whitespace-only lines */
    {
        const char *p;
        p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0' || *p == '\n') return 0;
    }

    /* Variable expansion */
    if (sh_expand(line, exp_buf, EXPAND_MAX) < 0)
    {
        sys_putstr("sh: expansion error\n");
        return 1;
    }

    /* Tokenize */
    ntoks = sh_lex(exp_buf, toks, TOK_MAX, tstore, TOK_STORE_SIZE);
    if (ntoks < 0)
    {
        sys_putstr("sh: syntax error\n");
        return 1;
    }
    if (ntoks == 0) return 0;

    /* Glob expansion */
    {
        char glob_store[TOK_STORE_SIZE];
        int expanded;
        expanded = glob_expand(toks, ntoks, TOK_MAX, glob_store, TOK_STORE_SIZE);
        if (expanded >= 0) ntoks = expanded;
    }

    /* Parse */
    if (sh_parse(toks, &chain) < 0)
        return 1;
    if (chain.n_pipes == 0) return 0;

    /* Execute */
    run_chain(&chain);
    return last_exit;
}

/*
 * Top-level execute: handles control flow keywords by entering
 * multi-line collection mode, or delegates to execute_line().
 */
static int  collecting;        /* 1 if collecting multi-line block */
static int  collect_depth;     /* nesting depth */
static char *script_lines[SCRIPT_LINES_MAX];
static int  script_nlines;

static void execute(char *line)
{
    /* Strip trailing newline */
    {
        int i;
        for (i = 0; line[i]; i++)
        {
            if (line[i] == '\n') { line[i] = '\0'; break; }
        }
    }

    /* Skip empty */
    {
        const char *p;
        p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0') return;
    }

    /* If currently collecting a multi-line block */
    if (collecting)
    {
        int llen;
        llen = sh_strlen(line);

        /* Store this line in script_buf */
        if (script_len + llen + 1 > SCRIPT_MAX)
        {
            sys_putstr("sh: script too long\n");
            collecting = 0;
            return;
        }
        if (script_nlines >= SCRIPT_LINES_MAX)
        {
            sys_putstr("sh: too many lines\n");
            collecting = 0;
            return;
        }

        sh_strncpy(&script_buf[script_len], line, SCRIPT_MAX - script_len);
        script_lines[script_nlines] = &script_buf[script_len];
        script_nlines++;
        script_len += llen + 1;

        /* Track depth */
        if (first_word_is(line, "if")) collect_depth++;
        if (first_word_is(line, "for")) collect_depth++;
        if (first_word_is(line, "while")) collect_depth++;
        if (first_word_is(line, "fi")) collect_depth--;
        if (first_word_is(line, "done")) collect_depth--;

        if (collect_depth <= 0)
        {
            collecting = 0;
            execute_lines((const char **)script_lines, script_nlines);
        }
        return;
    }

    /* Check if this line starts a control flow block */
    if (first_word_is(line, "if") ||
        first_word_is(line, "for") ||
        first_word_is(line, "while"))
    {
        int llen;
        llen = sh_strlen(line);

        collecting = 1;
        collect_depth = 1;
        script_len = 0;
        script_nlines = 0;

        sh_strncpy(script_buf, line, SCRIPT_MAX);
        script_lines[0] = script_buf;
        script_nlines = 1;
        script_len = llen + 1;
        return;
    }

    /* Regular single-line command */
    execute_line(line);
}

/* ================================================================
 *                     COMMAND HISTORY
 * ================================================================ */

#define HIST_MAX   32

static char hist_buf[HIST_MAX][INPUT_MAX];
static int  hist_count;     /* total entries stored */
static int  hist_head;      /* next write slot (ring) */

static void hist_add(const char *line)
{
    /* Skip empty lines */
    if (!line[0]) return;

    /* Skip duplicates of the most recent entry */
    if (hist_count > 0)
    {
        int prev;
        prev = (hist_head + HIST_MAX - 1) % HIST_MAX;
        if (sh_streq(hist_buf[prev], line)) return;
    }

    sh_strncpy(hist_buf[hist_head], line, INPUT_MAX);
    hist_head = (hist_head + 1) % HIST_MAX;
    if (hist_count < HIST_MAX) hist_count++;
}

static const char *hist_get(int idx)
{
    int slot;
    if (idx < 0 || idx >= hist_count) return (const char *)0;
    if (hist_count < HIST_MAX)
        slot = idx;
    else
        slot = (hist_head + idx) % HIST_MAX;
    return hist_buf[slot];
}

/* "history" builtin — show recent commands */
static int bi_history(int argc, char **argv)
{
    int i;
    (void)argc; (void)argv;
    for (i = 0; i < hist_count; i++)
    {
        print_int(i + 1);
        sys_putstr("  ");
        sys_putstr(hist_get(i));
        sys_putc('\n');
    }
    return 0;
}

/* ================================================================
 *                     TAB COMPLETION
 * ================================================================ */

/* Forward declaration (defined in RAW LINE EDITOR section below) */
static void le_redraw(const char *prompt, char *buf, int len, int cursor);

#define COMP_MAX        32
#define COMP_NAME_LEN   17
#define COMP_DIR_ENTRIES 64
#define COMP_DIR_WORDS   8
#define COMP_FNAME_WORDS 4

static char comp_matches[COMP_MAX][COMP_NAME_LEN];
static int  comp_count;

static void comp_decompress(char *dest, unsigned int *src)
{
    int wi;
    unsigned int word;
    unsigned int c;
    int ci;

    ci = 0;
    for (wi = 0; wi < COMP_FNAME_WORDS; wi++)
    {
        word = src[wi];
        c = (word >> 24) & 0xFF;
        dest[ci++] = (char)c;
        if (c == 0) return;
        c = (word >> 16) & 0xFF;
        dest[ci++] = (char)c;
        if (c == 0) return;
        c = (word >> 8) & 0xFF;
        dest[ci++] = (char)c;
        if (c == 0) return;
        c = word & 0xFF;
        dest[ci++] = (char)c;
        if (c == 0) return;
    }
    dest[ci] = '\0';
}

/* Check if 'name' starts with 'prefix' (case-sensitive) */
static int starts_with(const char *name, const char *prefix)
{
    while (*prefix)
    {
        if (*name != *prefix) return 0;
        name++;
        prefix++;
    }
    return 1;
}

/* Add a match if it starts with prefix and there's room */
static void comp_add(const char *name, const char *prefix)
{
    if (comp_count >= COMP_MAX) return;
    if (!starts_with(name, prefix)) return;
    /* Skip . and .. */
    if (name[0] == '.' && (name[1] == '\0' ||
        (name[1] == '.' && name[2] == '\0'))) return;
    sh_strncpy(comp_matches[comp_count], name, COMP_NAME_LEN);
    comp_count++;
}

/* Find longest common prefix among matches, store in dst */
static int comp_common_prefix(char *dst, int max)
{
    int i;
    int j;

    if (comp_count == 0) return 0;
    sh_strncpy(dst, comp_matches[0], max);

    for (i = 1; i < comp_count; i++)
    {
        for (j = 0; dst[j]; j++)
        {
            if (comp_matches[i][j] != dst[j])
            {
                dst[j] = '\0';
                break;
            }
        }
    }
    return sh_strlen(dst);
}

/* Complete commands: builtins + /bin/ entries */
static void comp_commands(const char *prefix)
{
    int i;
    unsigned int entry_buf[COMP_DIR_ENTRIES * COMP_DIR_WORDS];
    int count;
    char name[COMP_NAME_LEN];

    comp_count = 0;

    /* Builtins */
    for (i = 0; builtins[i].name; i++)
        comp_add(builtins[i].name, prefix);

    /* Also add keywords */
    comp_add("if", prefix);
    comp_add("then", prefix);
    comp_add("else", prefix);
    comp_add("elif", prefix);
    comp_add("fi", prefix);
    comp_add("for", prefix);
    comp_add("while", prefix);
    comp_add("do", prefix);
    comp_add("done", prefix);
    comp_add("exit", prefix);
    comp_add("halt", prefix);
    comp_add("history", prefix);

    /* /bin/ entries */
    count = sys_readdir("/bin", entry_buf, COMP_DIR_ENTRIES);
    for (i = 0; i < count && comp_count < COMP_MAX; i++)
    {
        unsigned int *entry;
        entry = entry_buf + (i * COMP_DIR_WORDS);
        comp_decompress(name, entry);
        comp_add(name, prefix);
    }
}

/* Complete filenames in a directory */
static void comp_files(const char *dir, const char *prefix)
{
    unsigned int entry_buf[COMP_DIR_ENTRIES * COMP_DIR_WORDS];
    int count;
    int i;
    char name[COMP_NAME_LEN];

    comp_count = 0;
    count = sys_readdir(dir, entry_buf, COMP_DIR_ENTRIES);
    for (i = 0; i < count && comp_count < COMP_MAX; i++)
    {
        unsigned int *entry;
        entry = entry_buf + (i * COMP_DIR_WORDS);
        comp_decompress(name, entry);
        comp_add(name, prefix);
    }
}

/*
 * Perform tab completion on the current input buffer.
 * Returns:
 *   0 = no change
 *   1 = completion applied (buffer/len/cursor updated)
 */
static int do_tab_complete(char *buf, int *len, int *cursor,
                           const char *prompt)
{
    int word_start;
    int is_cmd;
    char prefix[COMP_NAME_LEN];
    int plen;
    char dir[CWD_MAX];
    char common[COMP_NAME_LEN];
    int clen;
    int extra;
    int i;

    /* Find start of current word */
    word_start = *cursor;
    while (word_start > 0 &&
           buf[word_start - 1] != ' ' &&
           buf[word_start - 1] != '\t')
        word_start--;

    /* Extract prefix */
    plen = *cursor - word_start;
    if (plen >= COMP_NAME_LEN) return 0;
    for (i = 0; i < plen; i++)
        prefix[i] = buf[word_start + i];
    prefix[plen] = '\0';

    /* Determine if this is a command position */
    is_cmd = 1;
    {
        int j;
        j = word_start;
        while (j > 0 && (buf[j - 1] == ' ' || buf[j - 1] == '\t'))
            j--;
        if (j > 0)
        {
            char prev;
            prev = buf[j - 1];
            if (prev != '|' && prev != ';' && prev != '&')
                is_cmd = 0;
        }
    }

    if (is_cmd)
    {
        comp_commands(prefix);
    }
    else
    {
        /* Check if prefix contains a path separator */
        int last_slash;
        last_slash = -1;
        for (i = 0; i < plen; i++)
        {
            if (prefix[i] == '/') last_slash = i;
        }

        if (last_slash >= 0)
        {
            /* Directory prefix in the word */
            char file_prefix[COMP_NAME_LEN];
            int dlen;

            if (last_slash == 0)
            {
                dir[0] = '/';
                dir[1] = '\0';
            }
            else
            {
                for (i = 0; i < last_slash && i < CWD_MAX - 1; i++)
                    dir[i] = prefix[i];
                dir[i] = '\0';
            }

            dlen = plen - last_slash - 1;
            for (i = 0; i < dlen && i < COMP_NAME_LEN - 1; i++)
                file_prefix[i] = prefix[last_slash + 1 + i];
            file_prefix[i] = '\0';

            comp_files(dir, file_prefix);
            /* Adjust: only the file part needs completing */
            plen = dlen;
            word_start = *cursor - dlen;
        }
        else
        {
            /* Complete in cwd */
            comp_files(cwd, prefix);
        }
    }

    if (comp_count == 0) return 0;

    if (comp_count == 1)
    {
        /* Single match — complete it */
        clen = sh_strlen(comp_matches[0]);
        extra = clen - plen;
        if (*len + extra >= INPUT_MAX) return 0;

        /* Make room and insert */
        for (i = *len - 1; i >= *cursor; i--)
            buf[i + extra] = buf[i];
        for (i = 0; i < extra; i++)
            buf[*cursor + i] = comp_matches[0][plen + i];
        *len += extra;
        *cursor += extra;
        buf[*len] = '\0';
        le_redraw(prompt, buf, *len, *cursor);
        return 1;
    }

    /* Multiple matches — complete common prefix and show options */
    clen = comp_common_prefix(common, COMP_NAME_LEN);
    extra = clen - plen;
    if (extra > 0 && *len + extra < INPUT_MAX)
    {
        for (i = *len - 1; i >= *cursor; i--)
            buf[i + extra] = buf[i];
        for (i = 0; i < extra; i++)
            buf[*cursor + i] = common[plen + i];
        *len += extra;
        *cursor += extra;
        buf[*len] = '\0';
        le_redraw(prompt, buf, *len, *cursor);
        return 1;
    }

    /* Show all matches */
    sys_putc('\n');
    for (i = 0; i < comp_count; i++)
    {
        sys_putstr(comp_matches[i]);
        sys_putstr("  ");
    }
    sys_putc('\n');
    /* Redraw prompt and current line */
    sys_putstr(prompt);
    for (i = 0; i < *len; i++)
        sys_putc(buf[i]);
    /* Reposition cursor */
    {
        int back;
        back = *len - *cursor;
        while (back > 0) { sys_putstr("\x1b[D"); back--; }
    }
    return 0;
}

/* ================================================================
 *                     RAW LINE EDITOR
 * ================================================================ */

static int raw_fd;       /* fd for /dev/tty in raw+nonblock mode */

/* Erase the currently displayed line (from cursor back to col 0) and
 * redraw the full edit buffer. Cursor is placed at position 'cursor'. */
static void le_redraw(const char *prompt, char *buf, int len, int cursor)
{
    int i;
    /* CR, then print prompt */
    sys_putc('\r');
    sys_putstr(prompt);
    /* Print buffer */
    for (i = 0; i < len; i++)
        sys_putc(buf[i]);
    /* Clear from end of text to end of line (ESC [ K) */
    sys_putstr("\x1b[K");
    /* Move cursor back to correct position:
     * We are at prompt_len + len; need to be at prompt_len + cursor */
    {
        int back;
        back = len - cursor;
        while (back > 0) { sys_putstr("\x1b[D"); back--; }
    }
}

/*
 * Read a line using raw TTY input.  Handles:
 *   - printable char insert at cursor
 *   - backspace / delete
 *   - left / right arrow, home / end
 *   - up / down arrow for history
 *   - Ctrl-A (home), Ctrl-E (end), Ctrl-U (kill line), Ctrl-K (kill to end)
 *   - Ctrl-C (cancel line), Ctrl-D (exit on empty line)
 * Returns line length, or -1 for EOF (Ctrl-D on empty line).
 */
static int read_line_raw(char *prompt)
{
    int  len;
    int  cursor;
    int  hist_pos;    /* -1 = editing new line; 0..hist_count-1 = browsing */
    char saved[INPUT_MAX]; /* saved partial input when browsing history */
    int  saved_len;

    len = 0;
    cursor = 0;
    hist_pos = -1;
    saved[0] = '\0';
    saved_len = 0;
    input_buf[0] = '\0';

    while (1)
    {
        int key;
        key = sys_tty_event_read(raw_fd, 0);
        if (key < 0)
        {
            sys_sleep(10);
            continue;
        }

        /* ---- Enter ---- */
        if (key == '\n' || key == '\r')
        {
            input_buf[len] = '\0';
            sys_putc('\n');
            return len;
        }

        /* ---- Ctrl-C: cancel ---- */
        if (key == 3)
        {
            sys_putstr("^C\n");
            len = 0;
            cursor = 0;
            input_buf[0] = '\0';
            return 0;
        }

        /* ---- Ctrl-D: EOF on empty line ---- */
        if (key == 4)
        {
            if (len == 0) return -1;
            continue;
        }

        /* ---- Ctrl-U: kill whole line ---- */
        if (key == 21)
        {
            len = 0;
            cursor = 0;
            input_buf[0] = '\0';
            le_redraw(prompt, input_buf, len, cursor);
            continue;
        }

        /* ---- Ctrl-K: kill to end of line ---- */
        if (key == 11)
        {
            len = cursor;
            input_buf[len] = '\0';
            le_redraw(prompt, input_buf, len, cursor);
            continue;
        }

        /* ---- Ctrl-A / Home: beginning of line ---- */
        if (key == 1 || key == KEY_HOME)
        {
            cursor = 0;
            le_redraw(prompt, input_buf, len, cursor);
            continue;
        }

        /* ---- Ctrl-E / End: end of line ---- */
        if (key == 5 || key == KEY_END)
        {
            cursor = len;
            le_redraw(prompt, input_buf, len, cursor);
            continue;
        }

        /* ---- Backspace ---- */
        if (key == '\b' || key == 0x7F)
        {
            if (cursor > 0)
            {
                int i;
                for (i = cursor - 1; i < len - 1; i++)
                    input_buf[i] = input_buf[i + 1];
                len--;
                cursor--;
                input_buf[len] = '\0';
                le_redraw(prompt, input_buf, len, cursor);
            }
            continue;
        }

        /* ---- Delete ---- */
        if (key == KEY_DELETE)
        {
            if (cursor < len)
            {
                int i;
                for (i = cursor; i < len - 1; i++)
                    input_buf[i] = input_buf[i + 1];
                len--;
                input_buf[len] = '\0';
                le_redraw(prompt, input_buf, len, cursor);
            }
            continue;
        }

        /* ---- Left arrow ---- */
        if (key == KEY_LEFT)
        {
            if (cursor > 0)
            {
                cursor--;
                sys_putstr("\x1b[D");
            }
            continue;
        }

        /* ---- Right arrow ---- */
        if (key == KEY_RIGHT)
        {
            if (cursor < len)
            {
                cursor++;
                sys_putstr("\x1b[C");
            }
            continue;
        }

        /* ---- Up arrow: history previous ---- */
        if (key == KEY_UP)
        {
            int target;
            const char *entry;
            int elen;

            if (hist_count == 0) continue;

            if (hist_pos < 0)
            {
                /* Save current input */
                sh_strncpy(saved, input_buf, INPUT_MAX);
                saved_len = len;
                target = hist_count - 1;
            }
            else
            {
                target = hist_pos - 1;
            }

            if (target < 0) continue;

            entry = hist_get(target);
            if (!entry) continue;

            hist_pos = target;
            elen = sh_strlen(entry);
            sh_strncpy(input_buf, entry, INPUT_MAX);
            len = elen;
            cursor = elen;
            le_redraw(prompt, input_buf, len, cursor);
            continue;
        }

        /* ---- Down arrow: history next ---- */
        if (key == KEY_DOWN)
        {
            if (hist_pos < 0) continue;

            if (hist_pos >= hist_count - 1)
            {
                /* Restore saved input */
                hist_pos = -1;
                sh_strncpy(input_buf, saved, INPUT_MAX);
                len = saved_len;
                cursor = len;
                le_redraw(prompt, input_buf, len, cursor);
            }
            else
            {
                const char *entry;
                int elen;
                hist_pos++;
                entry = hist_get(hist_pos);
                if (!entry) continue;
                elen = sh_strlen(entry);
                sh_strncpy(input_buf, entry, INPUT_MAX);
                len = elen;
                cursor = elen;
                le_redraw(prompt, input_buf, len, cursor);
            }
            continue;
        }

        /* ---- Tab: completion ---- */
        if (key == '\t')
        {
            do_tab_complete(input_buf, &len, &cursor, prompt);
            continue;
        }

        /* ---- Printable character ---- */
        if (key >= 0x20 && key <= 0x7E)
        {
            if (len >= INPUT_MAX - 1) continue;

            /* Insert at cursor */
            if (cursor < len)
            {
                int i;
                for (i = len; i > cursor; i--)
                    input_buf[i] = input_buf[i - 1];
            }
            input_buf[cursor] = (char)key;
            len++;
            cursor++;
            input_buf[len] = '\0';

            /* Optimise: if cursor is at end, just echo the char */
            if (cursor == len)
            {
                sys_putc((char)key);
            }
            else
            {
                le_redraw(prompt, input_buf, len, cursor);
            }
            continue;
        }

        /* Ignore other keys */
    }
}

/* ================================================================
 *                     MAIN LOOP
 * ================================================================ */

int main(void)
{
    char prompt[CWD_MAX + 4];

    /* Ensure /tmp exists for pipe temp files */
    sys_mkdir("/tmp");

    /* Initialize cwd */
    sys_getcwd(cwd, CWD_MAX);

    /* Initialize variables */
    vars_init();

    /* Initialize history */
    hist_count = 0;
    hist_head = 0;

    /* Open raw TTY for line editing */
    raw_fd = sys_tty_open_raw(1);

    while (1)
    {
        int n;

        /* Build prompt */
        if (collecting)
        {
            prompt[0] = '>';
            prompt[1] = ' ';
            prompt[2] = '\0';
        }
        else
        {
            sh_strncpy(prompt, cwd, CWD_MAX);
            {
                int plen;
                plen = sh_strlen(prompt);
                prompt[plen] = '>';
                prompt[plen + 1] = ' ';
                prompt[plen + 2] = '\0';
            }
        }

        /* Show prompt */
        sys_putstr(prompt);

        /* Read a line with editing + history */
        n = read_line_raw(prompt);

        if (n < 0)
        {
            /* Ctrl-D on empty line — exit shell */
            sys_putstr("exit\n");
            sys_exit(0);
        }

        if (n > 0)
        {
            /* Add to history (before execution, so $? reflects prior cmd) */
            hist_add(input_buf);
        }

        /* Execute */
        execute(input_buf);

        /* Refresh cwd (may have changed via cd) */
        sys_getcwd(cwd, CWD_MAX);
    }
}
