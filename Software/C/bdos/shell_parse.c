/*
 * shell_parse.c \u2014 BDOS shell AST parser.
 *
 * Grammar:
 *   chain    := pipeline (op pipeline)*
 *   op       := && | || | ;
 *   pipeline := command (| command)*
 *   command  := WORD+ (redir)*
 *   redir    := < WORD | > WORD | >> WORD
 */

#ifdef SHELL_HOST_TEST
#include "shell_host_stubs.h"
#else
#include "bdos.h"
#endif

static void parse_error(const char *msg)
{
    term2_puts("shell: parse error: ");
    term2_puts(msg);
    term2_putchar('\n');
}

static int parse_command(sh_tok_t *toks, int *i, sh_cmd_t *cmd)
{
    int t;
    cmd->argc = 0;
    cmd->redir_in     = NULL;
    cmd->redir_out    = NULL;
    cmd->redir_append = NULL;

    while (1) {
        t = toks[*i].type;
        if (t == SH_TOK_END   || t == SH_TOK_PIPE  ||
            t == SH_TOK_AND   || t == SH_TOK_OR    ||
            t == SH_TOK_SEMI)
            break;

        if (t == SH_TOK_WORD) {
            if (cmd->argc >= BDOS_SHELL_ARGV_MAX) {
                parse_error("too many arguments");
                return -1;
            }
            cmd->argv[cmd->argc++] = toks[(*i)++].text;
            continue;
        }

        if (t == SH_TOK_REDIR_IN || t == SH_TOK_REDIR_OUT ||
            t == SH_TOK_REDIR_APPEND) {
            int op = t;
            (*i)++;
            if (toks[*i].type != SH_TOK_WORD) {
                parse_error("missing filename after redirect");
                return -1;
            }
            if (op == SH_TOK_REDIR_IN)        cmd->redir_in     = toks[*i].text;
            else if (op == SH_TOK_REDIR_OUT)  cmd->redir_out    = toks[*i].text;
            else                              cmd->redir_append = toks[*i].text;
            (*i)++;
            continue;
        }

        parse_error("unexpected token");
        return -1;
    }

    if (cmd->argc == 0) {
        parse_error("empty command");
        return -1;
    }
    /* NULL-terminate argv for downstream code that wants it. */
    if (cmd->argc < BDOS_SHELL_ARGV_MAX) cmd->argv[cmd->argc] = NULL;
    return 0;
}

static int parse_pipeline(sh_tok_t *toks, int *i, sh_pipeline_t *pl)
{
    pl->n_cmds = 0;
    while (1) {
        if (pl->n_cmds >= BDOS_SHELL_PIPE_MAX) {
            parse_error("pipeline too long");
            return -1;
        }
        if (parse_command(toks, i, &pl->cmds[pl->n_cmds]) < 0) return -1;
        pl->n_cmds++;
        if (toks[*i].type != SH_TOK_PIPE) break;
        (*i)++;
    }
    return 0;
}

int bdos_shell_parse(sh_tok_t *toks, sh_chain_t *out)
{
    int i = 0;
    out->n_pipes = 0;

    if (toks[0].type == SH_TOK_END) return 0;

    while (1) {
        int op;

        if (out->n_pipes >= BDOS_SHELL_CHAIN_MAX) {
            parse_error("chain too long");
            return -1;
        }
        if (parse_pipeline(toks, &i, &out->pipes[out->n_pipes]) < 0) return -1;

        op = toks[i].type;
        if      (op == SH_TOK_AND)  { out->ops[out->n_pipes++] = SH_OP_AND;  i++; }
        else if (op == SH_TOK_OR)   { out->ops[out->n_pipes++] = SH_OP_OR;   i++; }
        else if (op == SH_TOK_SEMI) { out->ops[out->n_pipes++] = SH_OP_SEMI; i++; }
        else if (op == SH_TOK_END)  { out->ops[out->n_pipes++] = SH_OP_END;  break; }
        else                        { parse_error("expected operator"); return -1; }

        if (toks[i].type == SH_TOK_END) {
            /* Trailing ; is allowed; trailing && / || is not. */
            if (op == SH_TOK_AND || op == SH_TOK_OR) {
                parse_error("trailing && / ||");
                return -1;
            }
            break;
        }
    }
    return 0;
}
