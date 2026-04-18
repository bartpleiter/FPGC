/*
 * shell_host_stubs.h — host-side replacement for "bdos.h" used when
 * compiling shell_lex.c / shell_parse.c / shell_vars.c into a desktop
 * gcc binary for unit testing.
 *
 * The real BDOS bdos.h pulls in libc + libfpgc + every kernel header,
 * which obviously can't link on a host. The shell modules themselves
 * only need: the bdos_shell.h public types/constants, ASCII helpers,
 * and a couple of stubbed-out kernel functions.
 */
#ifndef SHELL_HOST_STUBS_H
#define SHELL_HOST_STUBS_H

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* bdos_shell.h pulls BRFS constants from brfs.h on-target — fake them. */
#ifndef BRFS_MAX_PATH_LENGTH
#define BRFS_MAX_PATH_LENGTH 127
#endif
#ifndef BRFS_MAX_FILENAME_LENGTH
#define BRFS_MAX_FILENAME_LENGTH 31
#endif

#include "bdos_shell.h"

/* bdos_shell_last_exit is owned by the test driver TU. Each test main()
 * file should #define SHELL_HOST_TEST_MAIN before including this header
 * exactly once; the shell .c modules pull this header without that macro
 * and just see an extern declaration. */
#ifdef SHELL_HOST_TEST_MAIN
int bdos_shell_last_exit = 0;
#else
extern int bdos_shell_last_exit;
#endif

/* libterm v2 stubs — the host test doesn't need real output.
 * Marked unused so individual TUs that don't reference them don't warn. */
__attribute__((unused))
static void term2_puts(const char *s) { (void)s; }

__attribute__((unused))
static void term2_putchar(int c) { (void)c; }

/* shell_util helper — render an unsigned int into a fixed buffer.
 * The real implementation lives in shell_util.c; the simplest correct
 * version for testing is sprintf. Defined exactly once in the test
 * driver TU (the one that #defines SHELL_HOST_TEST_MAIN). */
#ifdef SHELL_HOST_TEST_MAIN
int bdos_shell_u32_to_str(unsigned int value, char *out)
{
    return sprintf(out, "%u", value);
}
#endif

#endif /* SHELL_HOST_STUBS_H */
