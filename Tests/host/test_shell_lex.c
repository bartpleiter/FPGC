/*
 * Host-side unit tests for shell_lex.c.
 *
 * Compile:
 *   gcc -O0 -Wall -DSHELL_HOST_TEST \
 *       -I Software/C/bdos/include -I Tests/host \
 *       Tests/host/test_shell_lex.c Software/C/bdos/shell_lex.c \
 *       -o /tmp/test_shell_lex
 *
 * Run: /tmp/test_shell_lex — exits 0 on success, nonzero on first failure.
 *
 * Coverage:
 *   - bare words, multiple words, leading/trailing whitespace
 *   - single-quoted strings (verbatim, no operator splitting)
 *   - double-quoted strings with \\ \" escape handling
 *   - backslash-escapes outside quotes
 *   - operators ; | || && < > >>
 *   - operator/word adjacency without separating spaces
 *   - empty input → just SH_TOK_END
 *   - comment handling (# starts a comment outside quotes)
 *   - max-tokens overflow returns -1
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

static int lex_ok(const char *line, sh_tok_t *toks)
{
    static char store[STORE];
    return bdos_shell_lex(line, toks, MAX_TOK, store, sizeof(store));
}

/* Each test uses its OWN store so different lex calls don't share buffers. */

static void test_empty(void)
{
    sh_tok_t toks[MAX_TOK];
    int n = lex_ok("", toks);
    CHECK(n == 0, "empty: got %d tokens", n);
    CHECK(toks[0].type == SH_TOK_END, "empty: type=%d", toks[0].type);
}

static void test_single_word(void)
{
    sh_tok_t toks[MAX_TOK];
    int n = lex_ok("ls", toks);
    CHECK(n == 1, "single: n=%d", n);
    CHECK(toks[0].type == SH_TOK_WORD, "single: type=%d", toks[0].type);
    CHECK(strcmp(toks[0].text, "ls") == 0, "single: text='%s'", toks[0].text);
}

static void test_multi_word(void)
{
    sh_tok_t toks[MAX_TOK];
    int n = lex_ok("  ls  -la   /bin  ", toks);
    CHECK(n == 3, "multi: n=%d", n);
    CHECK(strcmp(toks[0].text, "ls")   == 0, "multi[0]='%s'", toks[0].text);
    CHECK(strcmp(toks[1].text, "-la")  == 0, "multi[1]='%s'", toks[1].text);
    CHECK(strcmp(toks[2].text, "/bin") == 0, "multi[2]='%s'", toks[2].text);
}

static void test_single_quotes(void)
{
    sh_tok_t toks[MAX_TOK];
    /* Single quotes preserve everything verbatim — no escape, no operator. */
    int n = lex_ok("echo 'hello | world ; $foo'", toks);
    CHECK(n == 2, "sq: n=%d", n);
    CHECK(strcmp(toks[1].text, "hello | world ; $foo") == 0,
          "sq: text='%s'", toks[1].text);
}

static void test_double_quotes(void)
{
    sh_tok_t toks[MAX_TOK];
    /* Double quotes recognise \\, \", \$ but pass other chars literally. */
    int n = lex_ok("echo \"a\\\"b\\\\c\"", toks);
    CHECK(n == 2, "dq: n=%d", n);
    CHECK(strcmp(toks[1].text, "a\"b\\c") == 0, "dq: text='%s'", toks[1].text);
}

static void test_backslash_escape(void)
{
    sh_tok_t toks[MAX_TOK];
    /* Outside quotes, "\;" should be a literal ; in the word. */
    int n = lex_ok("foo\\;bar", toks);
    CHECK(n == 1, "bslash: n=%d", n);
    CHECK(strcmp(toks[0].text, "foo;bar") == 0, "bslash: text='%s'", toks[0].text);
}

static void test_operators(void)
{
    sh_tok_t toks[MAX_TOK];
    int n = lex_ok("a ; b | c && d || e", toks);
    CHECK(n == 9, "ops: n=%d", n);
    CHECK(toks[0].type == SH_TOK_WORD, "ops[0]");
    CHECK(toks[1].type == SH_TOK_SEMI, "ops[1]");
    CHECK(toks[2].type == SH_TOK_WORD, "ops[2]");
    CHECK(toks[3].type == SH_TOK_PIPE, "ops[3]");
    CHECK(toks[4].type == SH_TOK_WORD, "ops[4]");
    CHECK(toks[5].type == SH_TOK_AND,  "ops[5]");
    CHECK(toks[6].type == SH_TOK_WORD, "ops[6]");
    CHECK(toks[7].type == SH_TOK_OR,   "ops[7]");
    CHECK(toks[8].type == SH_TOK_WORD, "ops[8]");
}

static void test_redirs(void)
{
    sh_tok_t toks[MAX_TOK];
    int n = lex_ok("a < in > out >> log", toks);
    CHECK(n == 7, "redir: n=%d", n);
    CHECK(toks[1].type == SH_TOK_REDIR_IN,     "redir[1]=%d", toks[1].type);
    CHECK(toks[3].type == SH_TOK_REDIR_OUT,    "redir[3]=%d", toks[3].type);
    CHECK(toks[5].type == SH_TOK_REDIR_APPEND, "redir[5]=%d", toks[5].type);
}

static void test_adjacent(void)
{
    sh_tok_t toks[MAX_TOK];
    /* No spaces between words and operators. */
    int n = lex_ok("a|b>c", toks);
    CHECK(n == 5, "adj: n=%d", n);
    CHECK(toks[0].type == SH_TOK_WORD,      "adj[0]");
    CHECK(toks[1].type == SH_TOK_PIPE,      "adj[1]");
    CHECK(toks[2].type == SH_TOK_WORD,      "adj[2]");
    CHECK(toks[3].type == SH_TOK_REDIR_OUT, "adj[3]");
    CHECK(toks[4].type == SH_TOK_WORD,      "adj[4]");
    CHECK(strcmp(toks[2].text, "b") == 0,   "adj[2].text");
}

static void test_comment(void)
{
    sh_tok_t toks[MAX_TOK];
    int n = lex_ok("ls /bin # this is a comment", toks);
    CHECK(n == 2, "cmt: n=%d", n);
    CHECK(strcmp(toks[1].text, "/bin") == 0, "cmt: text='%s'", toks[1].text);

    /* Hash inside a word/quote should NOT start a comment. */
    n = lex_ok("a#b 'c # d'", toks);
    CHECK(n == 2, "cmt2: n=%d", n);
    CHECK(strcmp(toks[0].text, "a#b") == 0,    "cmt2[0]='%s'", toks[0].text);
    CHECK(strcmp(toks[1].text, "c # d") == 0,  "cmt2[1]='%s'", toks[1].text);
}

static void test_overflow(void)
{
    /* Pass a tiny store buffer to force the lexer to bail out. */
    sh_tok_t toks[MAX_TOK];
    char     tiny[4];
    int n = bdos_shell_lex("hello world", toks, MAX_TOK, tiny, sizeof(tiny));
    CHECK(n == -1, "overflow: n=%d", n);
}

int main(void)
{
    test_empty();
    test_single_word();
    test_multi_word();
    test_single_quotes();
    test_double_quotes();
    test_backslash_escape();
    test_operators();
    test_redirs();
    test_adjacent();
    test_comment();
    test_overflow();

    if (g_failures) {
        fprintf(stderr, "test_shell_lex: %d failure(s)\n", g_failures);
        return 1;
    }
    printf("test_shell_lex: all passed\n");
    return 0;
}
