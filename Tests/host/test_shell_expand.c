/*
 * Host-side unit tests for shell_vars.c — focuses on bdos_shell_expand.
 *
 * Compile:
 *   gcc -O0 -Wall -DSHELL_HOST_TEST \
 *       -I Software/C/bdos/include -I Tests/host \
 *       Tests/host/test_shell_expand.c \
 *       Software/C/bdos/shell_vars.c \
 *       -o /tmp/test_shell_expand
 *
 * Run: /tmp/test_shell_expand — exits 0 on success, nonzero on failure.
 *
 * Coverage:
 *   - $VAR plain expansion + ${VAR} braced form
 *   - undefined variable → empty string (no error)
 *   - $? from bdos_shell_last_exit
 *   - $#, $0..$9 from script_argc / script_argv
 *   - lone $ at end-of-string is literal
 *   - single-quoted segments are verbatim (no expansion)
 *   - double-quoted segments DO expand
 *   - \\ \" \$ escapes outside / inside quotes
 *   - dst overflow returns -1
 *   - unterminated quote returns -1
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

#define CHECK_STREQ(got, want) do { \
    if (strcmp((got), (want)) != 0) { \
        fprintf(stderr, "FAIL %s:%d: got=\"%s\" want=\"%s\"\n", \
                __FILE__, __LINE__, (got), (want)); \
        g_failures++; \
    } \
} while (0)

static void test_plain_var(void)
{
    char out[64];
    bdos_shell_var_set("FOO", "bar");
    int r = bdos_shell_expand("hello $FOO!", out, sizeof(out), 0, NULL);
    CHECK(r == 0, "plain: r=%d", r);
    CHECK_STREQ(out, "hello bar!");
}

static void test_braced_var(void)
{
    char out[64];
    bdos_shell_var_set("X", "abc");
    int r = bdos_shell_expand("${X}123", out, sizeof(out), 0, NULL);
    CHECK(r == 0, "braced: r=%d", r);
    CHECK_STREQ(out, "abc123");
}

static void test_undefined_is_empty(void)
{
    char out[64];
    int r = bdos_shell_expand("a${UNSET_VAR}b", out, sizeof(out), 0, NULL);
    CHECK(r == 0, "unset: r=%d", r);
    CHECK_STREQ(out, "ab");
}

static void test_question_mark(void)
{
    char out[64];
    bdos_shell_last_exit = 42;
    int r = bdos_shell_expand("rc=$?", out, sizeof(out), 0, NULL);
    CHECK(r == 0, "?: r=%d", r);
    CHECK_STREQ(out, "rc=42");
    bdos_shell_last_exit = 0;
}

static void test_script_args(void)
{
    /* Per shell_script.c convention: argv[0] is script name, argv[1..N]
     * are positional args. $# returns N (i.e. argc - 1). */
    char *argv[] = { "myscript", "alpha", "beta", "gamma" };
    int argc = 4;

    char out[64];
    int r = bdos_shell_expand("$0 $1 $2 $3 ($# args)", out, sizeof(out),
                              argc, argv);
    CHECK(r == 0, "args: r=%d", r);
    CHECK_STREQ(out, "myscript alpha beta gamma (3 args)");

    /* Out-of-range positional → empty string. */
    r = bdos_shell_expand("[$5]", out, sizeof(out), argc, argv);
    CHECK(r == 0, "args5: r=%d", r);
    CHECK_STREQ(out, "[]");
}

static void test_lone_dollar(void)
{
    char out[64];
    int r = bdos_shell_expand("a$", out, sizeof(out), 0, NULL);
    CHECK(r == 0, "lone$: r=%d", r);
    CHECK_STREQ(out, "a$");
}

static void test_single_quotes_verbatim(void)
{
    char out[64];
    bdos_shell_var_set("FOO", "bar");
    int r = bdos_shell_expand("'$FOO is $?'", out, sizeof(out), 0, NULL);
    CHECK(r == 0, "sq: r=%d", r);
    /* Single quotes pass through everything literally and the surrounding
     * quotes themselves are stripped. */
    CHECK_STREQ(out, "$FOO is $?");
}

static void test_double_quotes_expand(void)
{
    char out[64];
    bdos_shell_var_set("FOO", "world");
    int r = bdos_shell_expand("\"hello $FOO\"", out, sizeof(out), 0, NULL);
    CHECK(r == 0, "dq: r=%d", r);
    CHECK_STREQ(out, "hello world");
}

static void test_escapes(void)
{
    char out[64];
    bdos_shell_var_set("FOO", "bar");
    /* \$FOO outside quotes → literal $FOO; \\ → \. */
    int r = bdos_shell_expand("\\$FOO is \\\\ here", out, sizeof(out), 0, NULL);
    CHECK(r == 0, "esc: r=%d", r);
    CHECK_STREQ(out, "$FOO is \\ here");
}

static void test_overflow(void)
{
    char tiny[4];
    bdos_shell_var_set("FOO", "abcdefgh");
    int r = bdos_shell_expand("$FOO", tiny, sizeof(tiny), 0, NULL);
    CHECK(r == -1, "overflow: r=%d (expected -1)", r);
}

static void test_unterminated_quote(void)
{
    char out[64];
    int r = bdos_shell_expand("'oops", out, sizeof(out), 0, NULL);
    CHECK(r == -1, "unterm sq: r=%d", r);

    r = bdos_shell_expand("\"oops", out, sizeof(out), 0, NULL);
    CHECK(r == -1, "unterm dq: r=%d", r);
}

int main(void)
{
    bdos_shell_vars_init();

    test_plain_var();
    test_braced_var();
    test_undefined_is_empty();
    test_question_mark();
    test_script_args();
    test_lone_dollar();
    test_single_quotes_verbatim();
    test_double_quotes_expand();
    test_escapes();
    test_overflow();
    test_unterminated_quote();

    if (g_failures) {
        fprintf(stderr, "test_shell_expand: %d failure(s)\n", g_failures);
        return 1;
    }
    printf("test_shell_expand: all passed\n");
    return 0;
}
