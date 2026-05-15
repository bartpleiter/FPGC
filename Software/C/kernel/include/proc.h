/*
 * proc.h — process table, scheduler, and context switch.
 *
 * Cooperative multitasking with timer-based soft preemption.
 * Up to 16 processes. One RUNNING at a time; others are READY,
 * BLOCKED, or ZOMBIE.
 */
#ifndef KERNEL_PROC_H
#define KERNEL_PROC_H

#define MAX_PROCS         16
#define MAX_FDS           16
#define MAX_ARGV          16
#define PROC_NAME_LEN     32
#define PROC_CWD_LEN      128

/* Process states */
#define PROC_FREE       0    /* Slot is unused */
#define PROC_RUNNING    1    /* Currently executing on CPU */
#define PROC_READY      2    /* Runnable, waiting for scheduler */
#define PROC_BLOCKED    3    /* Waiting on I/O, timer, or waitpid */
#define PROC_ZOMBIE     4    /* Exited, waiting for parent to collect */

/* Block reasons (stored in blocked_on) */
#define BLOCK_NONE      0
#define BLOCK_SLEEP     1    /* Sleeping until wake_time */
#define BLOCK_WAITPID   2    /* Waiting for child exit */
#define BLOCK_PIPE_READ 3    /* Pipe empty, waiting for writer */
#define BLOCK_PIPE_WRITE 4   /* Pipe full, waiting for reader */

struct proc {
    int            pid;
    int            ppid;
    int            state;
    int            exit_code;

    /* Memory region */
    unsigned int   mem_base;
    unsigned int   mem_size;
    unsigned int   heap_break;   /* Current sbrk break (next free byte) */

    /* Saved CPU context (filled by context switch) */
    unsigned int   saved_regs[16];   /* r0-r15 (r0 always 0) */
    unsigned int   saved_pc;
    unsigned int   saved_hw_sp;
    unsigned int   saved_hw_stack[256];

    /* File descriptors — indexes into global open file table */
    int            fds[MAX_FDS];

    /* Process info */
    char           name[PROC_NAME_LEN];
    char           cwd[PROC_CWD_LEN];
    int            argc;
    char          *argv[MAX_ARGV];

    /* Scheduling */
    int            fg;             /* Owns the terminal? */
    int            blocked_reason;
    unsigned int   wake_time;      /* For BLOCK_SLEEP: microsecond target */
    int            wait_pid;       /* For BLOCK_WAITPID: which PID */
};

/* ---- Process table API ---- */

void proc_init(void);

/* Get the current running process (or NULL if idle). */
struct proc *proc_current(void);

/* Get process by pid (returns NULL if not found). */
struct proc *proc_by_pid(int pid);

/* Spawn a new process from a binary file. Returns PID or -1. */
int proc_spawn(const char *path, int argc, char **argv);

/* Current process yields the CPU to the next READY process. */
void proc_yield(void);

/* Current process exits with the given code. Does not return. */
void proc_exit(int code);

/* Wait for a child process to exit. Returns exit code.
 * Blocks (yields) until the child is ZOMBIE. */
int proc_waitpid(int pid);

/* Put current process to sleep for `ms` milliseconds. Yields. */
void proc_sleep_ms(unsigned int ms);

/* ---- Scheduler ---- */

/* Run the scheduler: pick next READY process and switch to it.
 * Called from yield, exit, and timer soft preemption. */
void sched_run(void);

/* Wake sleeping processes whose timers have expired.
 * Called from sched_tick and from the shell wait loop. */
void sched_wake_sleepers(void);

/* Timer tick handler — called from Timer 0 ISR at 100 Hz.
 * Wakes sleeping processes and sets soft preemption flag. */
void sched_tick(void);

/* ---- Context switch (assembly) ---- */

/* Save current context and restore target context.
 * Implemented in crt0_kernel.asm. */
extern void context_switch(struct proc *from, struct proc *to);

/* Enter a user process: saves kernel state, loads user state from
 * proc struct (via current_proc_regs_ptr), and jumps to user code.
 * saved_regs[15] is the jump target (entry point or resume address).
 * Returns when the process exits or blocks. */
extern void context_enter(void);

/* Called from EXIT syscall handler after proc_exit() cleanup.
 * Resets HW stack and returns to context_enter caller. */
extern void syscall_exit_to_kernel(void);

/* Exit code from context_enter (legacy, kept for compat) */
extern unsigned int context_enter_retval;

/* Pointer to current proc's saved_regs[0]. Must be set before
 * calling context_enter() or making a syscall. */
extern unsigned int current_proc_regs_ptr;

/* Flag: set to 1 by blocking syscalls (waitpid, sleep).
 * Checked by Return_Syscall asm to exit to kernel. */
extern int proc_was_blocked;

/* ---- Globals ---- */

/* Soft preemption flag (set by timer ISR, checked at syscall return) */
extern int sched_should_yield;

/* Current PID (-1 if idle) */
extern int current_pid;

#endif /* KERNEL_PROC_H */
