/*
 * shell_lex.c \u2014 BDOS shell tokenizer.
 *
 * Splits an input line into tokens. WORD tokens have their quotes
 * and escapes consumed (but variable expansion happens BEFORE the
 * lexer runs \u2014 see bdos_shell_expand). The result is a flat token
 * array that the parser then walks into an AST.
 *
 * Operators recognised: < > >> | || && ;
 * Comments: # at start of an unquoted token starts a comment that
 * runs to end of line (only outside quotes).
 */

#include "bdos.h"

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
    if (store_putc(store, off, store_size, 0) < 0) return -1;
    toks[*ti].type = SH_TOK_WORD;
    toks[*ti].text = &store[word_start];
    (*ti)++;
    return 0;
}

static int emit_op(sh_tok_t *toks, int *ti, int max, int type)
{
    if (*ti >= max - 1) return -1;
    toks[*ti].type = type;
    toks[*ti].text = NULL;
    (*ti)++;
    return 0;
}

int bdos_shell_lex(const char *line, sh_tok_t *out_toks, int max_toks,
                   char *store, int store_size)
{
    int   ti       = 0;
    int   off      = 0;
    int   in_word  = 0;
    int   word_start = 0;
    int   in_sq    = 0;
    int   in_dq    = 0;
    const char *p  = line;

    while (*p) {
        char c = *p;

        if (in_sq) {
            if (c == '\'') { in_sq = 0; p++; continue; }
            if (store_putc(store, &off, store_size, c) < 0) return -1;
            p++;
            continue;
        }

        if (in_dq) {
            if (c == '"') { in_dq = 0; p++; continue; }
            if (c == '\\' && p[1]) {
                if (store_putc(store, &off, store_size, p[1]) < 0) return -1;
                p += 2;
                continue;
            }
            if (store_putc(store, &off, store_size, c) < 0) return -1;
            p++;
            continue;
        }

        /* Outside quotes */

        if (c == ' ' || c == '\t') {
            if (in_word) {
                if (emit_word(out_toks, &ti, max_toks, store, &off, store_size,
                              word_start) < 0) return -1;
                in_word = 0;
            }
            p++;
            continue;
        }

        if (c == '#' && !in_word) {
            /* Rest of line is a comment. */
            break;
        }

        if (c == '\'' || c == '"') {
            if (!in_word) { in_word = 1; word_start = off; }
            if (c == '\'') in_sq = 1; else in_dq = 1;
            p++;
            continue;
        }

        if (c == '\\' && p[1]) {
            if (!in_word) { in_word = 1; word_start = off; }
            if (store_putc(store, &off, store_size, p[1]) < 0) return -1;
            p += 2;
            continue;
        }

        /* Operator characters break the current word. */
        if (c == '<' || c == '>' || c == '|' || c == '&' || c == ';') {
            if (in_word) {
                if (emit_word(out_toks, &ti, max_toks, store, &off, store_size,
                              word_start) < 0) return -1;
                in_word = 0;
            }
            if (c == '>' && p[1] == '>') {
                if (emit_op(out_toks, &ti, max_toks, SH_TOK_REDIR_APPEND) < 0)
                    return -1;
                p += 2;
            } else if (c == '|' && p[1] == '|') {
                if (emit_op(out_toks, &ti, max_toks, SH_TOK_OR) < 0) return -1;
                p += 2;
            } else if (c == '&' && p[1] == '&') {
                if (emit_op(out_toks, &ti, max_toks, SH_TOK_AND) < 0) return -1;
                p += 2;
            } else if (c == '<') {
                if (emit_op(out_toks, &ti, max_toks, SH_TOK_REDIR_IN) < 0) return -1;
                p++;
            } else if (c == '>') {
                if (emit_op(out_toks, &ti, max_toks, SH_TOK_REDIR_OUT) < 0) return -1;
                p++;
            } else if (c == '|') {
                if (emit_op(out_toks, &ti, max_toks, SH_TOK_PIPE) < 0) return -1;
                p++;
            } else if (c == ';') {
                if (emit_op(out_toks, &ti, max_toks, SH_TOK_SEMI) < 0) return -1;
                p++;
            } else {
                /* lone & \u2014 not supported in v2.0 (no background) */
                return -1;
            }
            continue;
        }

        /* Regular character */
        if (!in_word) { in_word = 1; word_start = off; }
        if (store_putc(store, &off, store_size, c) < 0) return -1;
        p++;
    }

    if (in_sq || in_dq) return -1;   /* unterminated quote */

    if (in_word) {
        if (emit_word(out_toks, &ti, max_toks, store, &off, store_size,
                      word_start) < 0) return -1;
    }

    out_toks[ti].type = SH_TOK_END;
    out_toks[ti].text = NULL;
    return ti;
}
