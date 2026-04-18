/*
 * Host-side unit tests for shell_parse.c.
 *
 * Compile:
 *   gcc -O0 -Wall -DSHELL_HOST_TEST \
 *       -I Software/C/bdos/include -I Tests/host \
 *       Tests/host/test_shell_parse.c \
 *       Software/C/bdos/shell_parse.c \
 *       Software/C/bdos/shell_lex.c \
 *       -o /tmp/test_shell_parse
 *
 * Run: /tmp/test_shell_parse — exits 0 on success, nonzero on first failure.
 *
 * Coverage:
 *   - simple command (one pipeline, one command, no redirs)
 *   - argv collection (multiple WORD tokens)
 *   - input/output/append redirections
 *   - pipelines (a | b | c) — n_cmds correct
 *   - chains with ; && || — ops slot ends with SH_OP_END
 *   - empty input parses cleanly into n_pipes==0
 *   - error cases: empty command between operators, missing redir target,
 *     trailing && / ||
 */

#define SHELL_HOST_TEST_MAIN
#include "shell_host_stubs.h"
#include <stdio.h>
#include <string.h>

static int g_failures = 0;

#define CHECK(cond, msg, ...) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL %s:%d: " msg "\n", \
                __FILE__, __LINE__, ##__VA_ARGS__); \
        g_failures++; \
    } \
} while (0)

#define MAX_TOK 32
#define STORE   512

static int parse_line(const char *line, sh_chain_t *chain)
{
    static sh_tok_t toks[MAX_TOK];
    static char     store[STORE];
    int n = bdos_shell_lex(line, toks, MAX_TOK, store, sizeof(store));
    if (n < 0) return -1;
    return bdos_shell_parse(toks, chain);
}

static void test_empty(void)
{
    sh_chain_t c;
    int r = parse_line("", &c);
    CHECK(r == 0, "empty: r=%d", r);
    CHECK(c.n_pipes == 0, "empty: n_pipes=%d", c.n_pipes);
}

static void test_simple_command(void)
{
    sh_chain_t c;
    int r = parse_line("ls -la /bin", &c);
    CHECK(r == 0, "simple: r=%d", r);
    CHECK(c.n_pipes == 1, "simple: n_pipes=%d", c.n_pipes);
    CHECK(c.pipes[0].n_cmds == 1, "simple: n_cmds=%d", c.pipes[0].n_cmds);
    CHECK(c.pipes[0].cmds[0].argc == 3, "simple: argc=%d", c.pipes[0].cmds[0].argc);
    CHECK(strcmp(c.pipes[0].cmds[0].argv[0], "ls")   == 0, "argv[0]");
    CHECK(strcmp(c.pipes[0].cmds[0].argv[1], "-la")  == 0, "argv[1]");
    CHECK(strcmp(c.pipes[0].cmds[0].argv[2], "/bin") == 0, "argv[2]");
    CHECK(c.ops[0] == SH_OP_END, "simple: op0=%d", c.ops[0]);
}

static void test_redirections(void)
{
    sh_chain_t c;
    int r = parse_line("cat < in.txt > out.txt", &c);
    CHECK(r == 0, "redir: r=%d", r);
    CHECK(c.pipes[0].cmds[0].argc == 1, "redir: argc=%d", c.pipes[0].cmds[0].argc);
    CHECK(c.pipes[0].cmds[0].redir_in     != NULL &&
          strcmp(c.pipes[0].cmds[0].redir_in,  "in.txt")  == 0, "redir_in");
    CHECK(c.pipes[0].cmds[0].redir_out    != NULL &&
          strcmp(c.pipes[0].cmds[0].redir_out, "out.txt") == 0, "redir_out");
    CHECK(c.pipes[0].cmds[0].redir_append == NULL, "redir_append nil");

    r = parse_line("echo hi >> log", &c);
    CHECK(r == 0, "append: r=%d", r);
    CHECK(c.pipes[0].cmds[0].redir_append != NULL &&
          strcmp(c.pipes[0].cmds[0].redir_append, "log") == 0, "redir_append");
}

static void test_pipeline(void)
{
    sh_chain_t c;
    int r = parse_line("a | b | c", &c);
    CHECK(r == 0, "pipe: r=%d", r);
    CHECK(c.n_pipes == 1, "pipe: n_pipes=%d", c.n_pipes);
    CHECK(c.pipes[0].n_cmds == 3, "pipe: n_cmds=%d", c.pipes[0].n_cmds);
    CHECK(strcmp(c.pipes[0].cmds[0].argv[0], "a") == 0, "pipe argv0");
    CHECK(strcmp(c.pipes[0].cmds[1].argv[0], "b") == 0, "pipe argv1");
    CHECK(strcmp(c.pipes[0].cmds[2].argv[0], "c") == 0, "pipe argv2");
}

static void test_chain(void)
{
    sh_chain_t c;
    int r = parse_line("a && b || c ; d", &c);
    CHECK(r == 0, "chain: r=%d", r);
    /* n_pipes is the chain length INCLUDING the final pipeline; ops is
     * indexed by pipeline position with SH_OP_END marking the last. */
    CHECK(c.n_pipes == 4, "chain: n_pipes=%d", c.n_pipes);
    CHECK(c.ops[0] == SH_OP_AND,  "chain ops[0]=%d", c.ops[0]);
    CHECK(c.ops[1] == SH_OP_OR,   "chain ops[1]=%d", c.ops[1]);
    CHECK(c.ops[2] == SH_OP_SEMI, "chain ops[2]=%d", c.ops[2]);
    CHECK(c.ops[3] == SH_OP_END,  "chain ops[3]=%d", c.ops[3]);
}

static void test_chain_with_pipeline(void)
{
    sh_chain_t c;
    int r = parse_line("ls /bin | grep cat && echo found", &c);
    CHECK(r == 0, "mixed: r=%d", r);
    CHECK(c.n_pipes == 2, "mixed: n_pipes=%d", c.n_pipes);
    CHECK(c.pipes[0].n_cmds == 2, "mixed[0].n_cmds=%d", c.pipes[0].n_cmds);
    CHECK(c.pipes[1].n_cmds == 1, "mixed[1].n_cmds=%d", c.pipes[1].n_cmds);
    CHECK(c.ops[0] == SH_OP_AND, "mixed ops[0]=%d", c.ops[0]);
    CHECK(c.ops[1] == SH_OP_END, "mixed ops[1]=%d", c.ops[1]);
}

static void test_trailing_semicolon(void)
{
    sh_chain_t c;
    int r = parse_line("ls ;", &c);
    CHECK(r == 0, "semi: r=%d (trailing ; is allowed)", r);
    CHECK(c.n_pipes == 1, "semi: n_pipes=%d", c.n_pipes);
}

static void test_error_empty_command(void)
{
    sh_chain_t c;
    int r = parse_line("ls ;; echo", &c);
    CHECK(r < 0, "empty cmd: expected error, r=%d", r);
}

static void test_error_missing_redir_target(void)
{
    sh_chain_t c;
    int r = parse_line("cat <", &c);
    CHECK(r < 0, "missing target: expected error, r=%d", r);
}

static void test_error_trailing_and(void)
{
    sh_chain_t c;
    int r = parse_line("a &&", &c);
    CHECK(r < 0, "trailing &&: expected error, r=%d", r);
}

int main(void)
{
    test_empty();
    test_simple_command();
    test_redirections();
    test_pipeline();
    test_chain();
    test_chain_with_pipeline();
    test_trailing_semicolon();
    test_error_empty_command();
    test_error_missing_redir_target();
    test_error_trailing_and();

    if (g_failures) {
        fprintf(stderr, "test_shell_parse: %d failure(s)\n", g_failures);
        return 1;
    }
    printf("test_shell_parse: all passed\n");
    return 0;
}
