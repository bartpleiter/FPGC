/*
 * sched.c — Cooperative round-robin scheduler.
 *
 * Phase 1: cooperative only (sched_tick checks sched_should_yield).
 * Phase 2 will add timer-based preemption via 100Hz soft tick.
 */
#include "kernel.h"

/* Timer-based sleep: check all BLOCKED/SLEEP processes. */
static void sched_wake_sleepers(void)
{
    unsigned int now;
    int i;
    struct proc *p;

    now = get_micros();
    for (i = 0; i < MAX_PROCS; i++)
    {
        p = proc_by_pid(i);
        if (p && p->state == PROC_BLOCKED
            && p->blocked_reason == BLOCK_SLEEP
            && now >= p->wake_time)
        {
            p->state = PROC_READY;
            p->blocked_reason = BLOCK_NONE;
        }
    }
}

/* Pick the next READY process to run (round-robin). */
static int sched_pick_next(void)
{
    int i;
    int start;

    start = (current_pid + 1) % MAX_PROCS;
    for (i = 0; i < MAX_PROCS; i++)
    {
        int pid;
        struct proc *p;

        pid = (start + i) % MAX_PROCS;
        p = proc_by_pid(pid);
        if (p && p->state == PROC_READY)
            return pid;
    }

    /* No READY processes — stay on current if it's RUNNING */
    {
        struct proc *cur;
        cur = proc_current();
        if (cur && cur->state == PROC_RUNNING)
            return current_pid;
    }

    /* Nothing to run — return kernel (pid 0) */
    return 0;
}

void sched_tick(void)
{
    int next;
    struct proc *cur;
    struct proc *nxt;

    /* Wake sleeping processes */
    sched_wake_sleepers();

    /* Only switch if yield was requested */
    if (!sched_should_yield)
        return;
    sched_should_yield = 0;

    next = sched_pick_next();
    if (next == current_pid)
        return; /* No switch needed */

    cur = proc_current();
    nxt = proc_by_pid(next);
    if (!nxt) return;

    /* Mark current as READY (unless it's BLOCKED or ZOMBIE) */
    if (cur && cur->state == PROC_RUNNING)
        cur->state = PROC_READY;

    nxt->state = PROC_RUNNING;
    current_pid = next;

    /* Perform context switch */
    if (cur && cur->pid != 0 && nxt->pid != 0)
    {
        /* User→User: full register save/restore */
        context_switch(cur, nxt);
    }
    else if (cur && cur->pid == 0 && nxt->pid != 0)
    {
        /* Kernel→User: save kernel state, enter user process */
        context_enter(nxt);
    }
    else if (nxt->pid == 0)
    {
        /* Returning to kernel — just return from this function,
         * kernel_loop will continue where it left off */
    }
}

void sched_run(void)
{
    /* Start the scheduler — called once during boot.
     * For Phase 1, this is a no-op since the shell runs
     * in the kernel's main loop. */
}
