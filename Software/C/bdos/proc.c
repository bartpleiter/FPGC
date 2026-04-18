/*
 * proc.c \u2014 BDOS process model (Phase C of shell-terminal-v2).
 *
 * v2.0 has only synchronous foreground execution: proc_spawn allocates
 * a child entry, copies argv into a fresh per-proc arena, flips the
 * "current proc" pointer, runs bdos_exec_program, then restores state.
 *
 * The shell occupies PID 0 and is always RUNNING. Child PIDs are
 * monotonically assigned (1, 2, 3, \u2026) for the lifetime of BDOS.
 */

#include "bdos.h"
#include "bdos_proc.h"

#ifndef NULL
#define NULL ((void *)0)
#endif

static bdos_proc_t g_procs[BDOS_PROC_MAX];
static int         g_current_pid  = BDOS_SHELL_PID;
static int         g_next_pid     = 1;

/* ---- Local string helpers (no deps on libc) ---- */

static unsigned int s_strlen(const char *s)
{
    unsigned int n = 0;
    while (s[n]) n++;
    return n;
}

static void s_memcpy(char *dst, const char *src, unsigned int n)
{
    unsigned int i;
    for (i = 0; i < n; i++) dst[i] = src[i];
}

static void s_strncpy(char *dst, const char *src, unsigned int max)
{
    unsigned int i;
    for (i = 0; i < max - 1 && src[i]; i++) dst[i] = src[i];
    dst[i] = 0;
}

/* ---- Internals ---- */

static int alloc_pid(void)
{
    int i;
    for (i = 1; i < BDOS_PROC_MAX; i++) {
        if (g_procs[i].state == BDOS_PROC_FREE) {
            g_procs[i].state = BDOS_PROC_RUNNING;
            g_procs[i].pid   = g_next_pid++;
            return i;
        }
    }
    return -1;
}

/*
 * Inherit fds 0/1/2 from the parent. Other fds start unused so that
 * file handles opened by the parent don\u2019t leak into children. (Phase D
 * shell will dup2 fresh fds for redirections before invoking spawn.)
 */
static void inherit_stdio(bdos_proc_t *child, const bdos_proc_t *parent)
{
    int i;
    for (i = 0; i < BDOS_FD_MAX; i++) child->fds[i].in_use = 0;
    for (i = 0; i < 3; i++) child->fds[i] = parent->fds[i];
}

/* ---- Public API ---- */

void bdos_proc_init(void)
{
    int i;
    bdos_proc_t *shell;

    for (i = 0; i < BDOS_PROC_MAX; i++) {
        g_procs[i].state = BDOS_PROC_FREE;
        g_procs[i].pid   = -1;
        g_procs[i].slot  = -1;
        g_procs[i].arena = NULL;
        g_procs[i].arena_used = 0;
    }

    /* PID 0: the shell. Always running, no underlying slot. */
    shell = &g_procs[0];
    shell->state     = BDOS_PROC_RUNNING;
    shell->pid       = BDOS_SHELL_PID;
    shell->ppid      = 0;
    shell->slot      = -1;
    shell->exit_code = 0;
    shell->argc      = 0;
    s_strncpy(shell->name, "bdos-shell", sizeof(shell->name));

    g_current_pid = BDOS_SHELL_PID;

    /* Initialize the shell\u2019s fd table (pre-opens 0/1/2 \u2192 /dev/tty)
     * via the existing vfs init path, then switch the VFS over to
     * routing through per-process tables permanently. */
    bdos_vfs_init();
    bdos_vfs_use_proc_tables();
    /* Re-init the shell\u2019s table now that the VFS routes through it. */
    bdos_vfs_init();
}

bdos_proc_t *bdos_proc_current(void)
{
    return &g_procs[g_current_pid];
}

int bdos_proc_current_pid(void)
{
    return g_current_pid;
}

int bdos_proc_spawn(const char *path, int argc, char **argv)
{
    int            child_idx;
    int            saved_pid;
    int            i;
    int            exit_code;
    unsigned int   heap_mark;
    bdos_proc_t   *parent;
    bdos_proc_t   *child;
    char          *arena;
    unsigned int   arena_off;
    unsigned int   arena_words;
    char           path_buf[BDOS_SHELL_PATH_MAX];
    char          *p;

    if (!path || argc < 0 || argc > BDOS_PROC_ARGV_MAX) return -1;

    parent    = bdos_proc_current();
    child_idx = alloc_pid();
    if (child_idx < 0) return -1;

    child = &g_procs[child_idx];
    child->ppid      = parent->pid;
    child->slot      = -1;
    child->exit_code = 0;
    child->argc      = argc;

    /* Copy program name (basename for display). */
    {
        const char *base = path;
        const char *q;
        for (q = path; *q; q++) if (*q == '/') base = q + 1;
        s_strncpy(child->name, base, sizeof(child->name));
    }

    /* Allocate per-proc arena from BDOS heap and copy argv strings.
     * arena_size is in words; convert. */
    heap_mark = bdos_heap_mark();
    arena_words = (BDOS_PROC_ARENA_SIZE + 3) / 4;
    arena = (char *)bdos_heap_alloc(arena_words);
    if (!arena) {
        child->state = BDOS_PROC_FREE;
        return -1;
    }
    child->arena      = arena;
    child->arena_used = 0;

    arena_off = 0;
    for (i = 0; i < argc; i++) {
        unsigned int len = s_strlen(argv[i]) + 1;
        if (arena_off + len > BDOS_PROC_ARENA_SIZE) {
            /* Out of arena space: truncate argv. */
            child->argc = i;
            break;
        }
        s_memcpy(arena + arena_off, argv[i], len);
        child->argv[i] = arena + arena_off;
        arena_off += len;
    }
    child->arena_used = arena_off;

    /* Inherit stdio from parent. */
    inherit_stdio(child, parent);

    /* bdos_exec_program writes to its argument; copy into a writable
     * buffer because some callers pass string literals. */
    s_strncpy(path_buf, path, sizeof(path_buf));
    /* trim possible NUL-fill from strncpy semantics not relevant here */
    p = path_buf;

    /* Switch current proc to child for the duration of exec. */
    saved_pid     = g_current_pid;
    g_current_pid = child_idx;

    /* Mirror argv into the legacy shell-globals so the existing
     * SYSCALL_SHELL_ARGC/ARGV path keeps working unchanged.
     * (Phase D shell v2 will read directly from current proc.) */
    bdos_shell_prog_argc = child->argc;
    for (i = 0; i < child->argc && i < BDOS_SHELL_ARGV_MAX; i++)
        bdos_shell_prog_argv[i] = child->argv[i];

    exit_code = bdos_exec_program(p);

    g_current_pid = saved_pid;

    /* Tear down the child. */
    child->exit_code = exit_code;
    child->state     = BDOS_PROC_FREE;
    child->arena     = NULL;
    child->arena_used = 0;
    bdos_heap_release(heap_mark);

    /* Reset legacy globals so a stale pointer can\u2019t be dereferenced. */
    bdos_shell_prog_argc = 0;

    return exit_code;
}
