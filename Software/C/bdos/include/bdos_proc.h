#ifndef BDOS_PROC_H
#define BDOS_PROC_H

#include "bdos_vfs.h"
#include "bdos_mem_map.h"
#include "bdos_shell.h"

/*
 * BDOS process model (Phase C).
 *
 * v2.0 model: synchronous foreground execution. The shell is PID 0
 * and is always present. Each spawned child gets a proc table entry
 * for the duration of its run. proc_spawn = fork + exec + wait.
 *
 * Per-process state lives in bdos_proc_t:
 *   - argv[]/argv strings copied into a fresh per-proc arena (freed
 *     on exit), so the child's argv pointers no longer alias the
 *     shell's input buffer.
 *   - fds[] is the fd table the VFS uses while this proc is current.
 *     fds 0/1/2 are inherited from the parent on spawn.
 *
 * Shell-as-parent invariant: g_current_proc == 0 whenever the shell
 * is running. proc_spawn flips it to the child PID for the duration
 * of bdos_exec_program(), then flips back.
 */

#define BDOS_PROC_MAX        MEM_SLOT_COUNT     /* 8 */
#define BDOS_PROC_ARGV_MAX   BDOS_SHELL_ARGV_MAX
#define BDOS_PROC_ARENA_SIZE 1024                /* bytes for argv strings */

#define BDOS_PROC_FREE       0
#define BDOS_PROC_RUNNING    1

#define BDOS_SHELL_PID       0

typedef struct bdos_proc_s {
    int          state;      /* BDOS_PROC_FREE / BDOS_PROC_RUNNING */
    int          pid;
    int          ppid;
    int          slot;       /* underlying program slot, -1 for shell */
    int          exit_code;
    char         name[32];

    int          argc;
    char        *argv[BDOS_PROC_ARGV_MAX];

    /* Arena holding argv strings; allocated on spawn, freed on exit. */
    char        *arena;
    unsigned int arena_used;

    /* Per-process fd table (managed by vfs.c via bdos_proc_current). */
    bdos_fd_t    fds[BDOS_FD_MAX];
} bdos_proc_t;

void          bdos_proc_init(void);

bdos_proc_t  *bdos_proc_current(void);
int           bdos_proc_current_pid(void);

/*
 * Synchronous spawn: copies argv into a fresh arena, sets up fds,
 * runs bdos_exec_program(path), captures exit code, frees arena,
 * returns the exit code (or -1 on spawn failure).
 *
 * argv must be a NULL-terminated-by-count array (argc values).
 * Strings are duplicated into the child's arena so the caller can
 * reuse the source buffer.
 */
int           bdos_proc_spawn(const char *path, int argc, char **argv);

#endif /* BDOS_PROC_H */
