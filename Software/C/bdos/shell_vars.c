/*
 * shell_vars.c \u2014 BDOS shell variable table + $-expansion.
 *
 * Linear table; lookups are case-sensitive. Special variables ($?, $$,
 * $#, $0..$9) are computed on-the-fly inside the expander rather than
 * stored in the table.
 *
 * Quoting:
 *   '...'   verbatim, no expansion (backslashes also literal)
 *   "..."   $-expansion happens; \" \\ \$ are recognised
 *   bare    $-expansion happens; whitespace separates words (handled
 *           by the lexer, not here)
 */

#include "bdos.h"

typedef struct {
    int  in_use;
    int  exported;
    char name[BDOS_SHELL_VAR_NAME];
    char value[BDOS_SHELL_VAR_VALUE];
} shell_var_t;

static shell_var_t g_vars[BDOS_SHELL_VAR_MAX];

/* ---- Local helpers ---- */

static int s_streq(const char *a, const char *b)
{
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

static int s_strlen(const char *s)
{
    int n = 0;
    while (s[n]) n++;
    return n;
}

static void s_strncpy(char *dst, const char *src, int max)
{
    int i;
    for (i = 0; i < max - 1 && src[i]; i++) dst[i] = src[i];
    dst[i] = 0;
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

static shell_var_t *find_var(const char *name)
{
    int i;
    for (i = 0; i < BDOS_SHELL_VAR_MAX; i++) {
        if (g_vars[i].in_use && s_streq(g_vars[i].name, name))
            return &g_vars[i];
    }
    return NULL;
}

static shell_var_t *alloc_var(void)
{
    int i;
    for (i = 0; i < BDOS_SHELL_VAR_MAX; i++) {
        if (!g_vars[i].in_use) return &g_vars[i];
    }
    return NULL;
}

/* ---- Public API ---- */

void bdos_shell_vars_init(void)
{
    int i;
    for (i = 0; i < BDOS_SHELL_VAR_MAX; i++)
        g_vars[i].in_use = 0;

    bdos_shell_var_set("PATH", "/bin");
    bdos_shell_var_export("PATH");
    bdos_shell_var_set("HOME", "/");
    bdos_shell_var_export("HOME");
    bdos_shell_var_set("PS1", "");   /* empty \u2192 default "<cwd>> " in shell.c */
}

const char *bdos_shell_var_get(const char *name)
{
    shell_var_t *v = find_var(name);
    return v ? v->value : NULL;
}

int bdos_shell_var_set(const char *name, const char *value)
{
    shell_var_t *v;
    if (!name || !*name) return -1;
    if (s_strlen(name)  >= BDOS_SHELL_VAR_NAME)  return -1;
    if (s_strlen(value) >= BDOS_SHELL_VAR_VALUE) return -1;

    v = find_var(name);
    if (!v) {
        v = alloc_var();
        if (!v) return -1;
        v->in_use   = 1;
        v->exported = 0;
        s_strncpy(v->name, name, BDOS_SHELL_VAR_NAME);
    }
    s_strncpy(v->value, value, BDOS_SHELL_VAR_VALUE);
    return 0;
}

int bdos_shell_var_export(const char *name)
{
    shell_var_t *v = find_var(name);
    if (!v) {
        if (bdos_shell_var_set(name, "") < 0) return -1;
        v = find_var(name);
        if (!v) return -1;
    }
    v->exported = 1;
    return 0;
}

int bdos_shell_var_unset(const char *name)
{
    shell_var_t *v = find_var(name);
    if (!v) return -1;
    v->in_use = 0;
    return 0;
}

void bdos_shell_vars_foreach(int exported_only,
                             void (*cb)(const char *, const char *, int))
{
    int i;
    for (i = 0; i < BDOS_SHELL_VAR_MAX; i++) {
        if (!g_vars[i].in_use) continue;
        if (exported_only && !g_vars[i].exported) continue;
        cb(g_vars[i].name, g_vars[i].value, g_vars[i].exported);
    }
}

static void print_var_cb(const char *name, const char *value, int exported)
{
    (void)exported;
    term2_puts(name);
    term2_putchar('=');
    term2_puts(value);
    term2_putchar('\n');
}

void bdos_shell_vars_print(int exported_only)
{
    bdos_shell_vars_foreach(exported_only, print_var_cb);
}

/* ---- $-expansion ---- */

static int append_str(char *dst, int *off, int dst_size, const char *s)
{
    while (*s) {
        if (*off >= dst_size - 1) return -1;
        dst[(*off)++] = *s++;
    }
    return 0;
}

static const char *lookup_special(char c, int script_argc, char **script_argv,
                                  char *scratch)
{
    if (c == '?') {
        bdos_shell_u32_to_str((unsigned int)bdos_shell_last_exit, scratch);
        return scratch;
    }
    if (c == '$') {
        bdos_shell_u32_to_str(0, scratch);   /* shell PID */
        return scratch;
    }
    if (c == '#') {
        int n = (script_argc > 0) ? script_argc - 1 : 0;
        bdos_shell_u32_to_str((unsigned int)n, scratch);
        return scratch;
    }
    if (c >= '0' && c <= '9') {
        int idx = c - '0';
        if (idx < script_argc) return script_argv[idx];
        return "";
    }
    return NULL;
}

int bdos_shell_expand(const char *src, char *dst, int dst_size,
                      int script_argc, char **script_argv)
{
    int  off  = 0;
    int  in_sq = 0;       /* inside single quotes */
    int  in_dq = 0;       /* inside double quotes */
    char num_buf[16];

    while (*src) {
        char c = *src;

        if (in_sq) {
            if (c == '\'') { in_sq = 0; src++; continue; }
            if (off >= dst_size - 1) return -1;
            dst[off++] = c;
            src++;
            continue;
        }

        if (c == '\'' && !in_dq) { in_sq = 1; src++; continue; }
        if (c == '"')             { in_dq = !in_dq; src++; continue; }

        if (c == '\\' && src[1]) {
            /* Escape: pass through next char literally */
            if (off >= dst_size - 1) return -1;
            dst[off++] = src[1];
            src += 2;
            continue;
        }

        if (c == '$' && src[1]) {
            const char *val = NULL;
            char        name_buf[BDOS_SHELL_VAR_NAME];
            int         nlen = 0;

            src++;
            if (*src == '{') {
                src++;
                while (*src && *src != '}' && nlen < BDOS_SHELL_VAR_NAME - 1)
                    name_buf[nlen++] = *src++;
                name_buf[nlen] = 0;
                if (*src == '}') src++;
                val = bdos_shell_var_get(name_buf);
            } else {
                val = lookup_special(*src, script_argc, script_argv, num_buf);
                if (val) {
                    src++;
                } else if (is_name_start(*src)) {
                    while (is_name_char(*src) && nlen < BDOS_SHELL_VAR_NAME - 1)
                        name_buf[nlen++] = *src++;
                    name_buf[nlen] = 0;
                    val = bdos_shell_var_get(name_buf);
                } else {
                    /* Lone $ \u2014 emit literal */
                    if (off >= dst_size - 1) return -1;
                    dst[off++] = '$';
                    continue;
                }
            }
            if (val && append_str(dst, &off, dst_size, val) < 0) return -1;
            continue;
        }

        if (off >= dst_size - 1) return -1;
        dst[off++] = c;
        src++;
    }

    if (in_sq || in_dq) return -1;   /* unterminated quote */
    dst[off] = 0;
    return 0;
}
