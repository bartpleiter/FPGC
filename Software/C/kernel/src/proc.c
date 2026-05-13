/*
 * proc.c — Process table management.
 *
 * Max 16 processes. Process 0 is the kernel/shell (Phase 1).
 * Variable-size memory allocation via mem.c.
 */
#include "kernel.h"

/* Process table */
static struct proc proc_table[MAX_PROCS];
int current_pid;
int sched_should_yield;

void proc_init(void)
{
    int i;
    int j;

    for (i = 0; i < MAX_PROCS; i++)
    {
        proc_table[i].pid = i;
        proc_table[i].state = PROC_FREE;
        proc_table[i].ppid = -1;
        proc_table[i].exit_code = 0;
        proc_table[i].mem_base = 0;
        proc_table[i].mem_size = 0;
        proc_table[i].heap_break = 0;
        proc_table[i].saved_pc = 0;
        proc_table[i].saved_hw_sp = 0;
        proc_table[i].fg = 0;
        proc_table[i].blocked_reason = BLOCK_NONE;
        proc_table[i].wake_time = 0;
        proc_table[i].wait_pid = -1;
        proc_table[i].argc = 0;
        proc_table[i].cwd[0] = '/';
        proc_table[i].cwd[1] = '\0';
        proc_table[i].name[0] = '\0';

        for (j = 0; j < MAX_FDS; j++)
            proc_table[i].fds[j] = -1;
        for (j = 0; j < 16; j++)
            proc_table[i].saved_regs[j] = 0;
        for (j = 0; j < 256; j++)
            proc_table[i].saved_hw_stack[j] = 0;
        for (j = 0; j < 16; j++)
            proc_table[i].argv[j] = (char *)0;
    }

    /* Process 0: kernel itself */
    proc_table[0].state = PROC_RUNNING;
    proc_table[0].name[0] = 'k';
    proc_table[0].name[1] = 'e';
    proc_table[0].name[2] = 'r';
    proc_table[0].name[3] = 'n';
    proc_table[0].name[4] = 'e';
    proc_table[0].name[5] = 'l';
    proc_table[0].name[6] = '\0';

    current_pid = 0;
    sched_should_yield = 0;
}

struct proc *proc_current(void)
{
    if (current_pid < 0 || current_pid >= MAX_PROCS)
        return 0;
    return &proc_table[current_pid];
}

struct proc *proc_by_pid(int pid)
{
    if (pid < 0 || pid >= MAX_PROCS)
        return 0;
    if (proc_table[pid].state == PROC_FREE)
        return 0;
    return &proc_table[pid];
}

int proc_spawn(const char *path, int argc, char **argv)
{
    int pid;
    int i;
    struct proc *p;
    struct proc *parent;
    const char *rel_path;
    struct brfs_state *fs;
    int brfs_fd;
    int file_size;
    unsigned int mem_base;
    unsigned int mem_size;

    /* Find a free slot */
    pid = -1;
    for (i = 1; i < MAX_PROCS; i++)
    {
        if (proc_table[i].state == PROC_FREE)
        {
            pid = i;
            break;
        }
    }
    if (pid < 0) return -1;

    /* Open the binary */
    fs = fs_for_path(path, &rel_path);
    if (!fs) return -1;

    brfs_fd = brfs_open(fs, rel_path);
    if (brfs_fd < 0) return -1;

    /* Get file size to determine memory needed */
    file_size = brfs_file_size(fs, brfs_fd);
    if (file_size <= 0)
    {
        brfs_close(fs, brfs_fd);
        return -1;
    }

    /* Allocate file_size + 8 MiB headroom for heap/stack.
     * Phase 1 is single-tasking, so generous allocation is fine.
     * Phase 2 will need smarter allocation. */
    mem_size = (unsigned int)file_size + (8u * 1024u * 1024u);
    if (mem_size < PROC_MEM_MIN)
        mem_size = PROC_MEM_MIN;

    mem_base = mem_alloc(mem_size);
    if (mem_base == 0)
    {
        brfs_close(fs, brfs_fd);
        return -1;
    }

    /* Load binary into allocated memory */
    {
        int bytes_read;
        brfs_seek(fs, brfs_fd, 0); /* Seek to start */
        bytes_read = brfs_read(fs, brfs_fd, (void *)mem_base, (unsigned int)file_size);
        if (bytes_read != file_size)
        {
            mem_free_region(mem_base, mem_size);
            brfs_close(fs, brfs_fd);
            return -1;
        }
    }
    brfs_close(fs, brfs_fd);

    /* Apply load-time relocations if present */
    {
        unsigned int *prog_base;
        unsigned int program_size;

        prog_base = (unsigned int *)mem_base;
        program_size = prog_base[2]; /* header word 2: program size in words */

        /* file_size is bytes, program_size is words */
        if ((unsigned int)file_size > program_size * 4u)
        {
            /* Relocation table present after program data */
            unsigned int delta;
            unsigned int reloc_count;
            unsigned int ri;

            delta = (unsigned int)mem_base; /* programs assembled with base 0 */
            reloc_count = prog_base[program_size];

            for (ri = 0; ri < reloc_count; ri++)
            {
                unsigned int entry;
                unsigned int byte_offset;
                unsigned int rtype;
                unsigned int word_idx;

                entry = prog_base[program_size + 1 + ri];
                byte_offset = entry >> 8;
                rtype = entry & 0xFF;
                word_idx = byte_offset / 4;

                if (rtype == 0)
                {
                    /* Type 0: data word — add delta to full 32-bit value */
                    prog_base[word_idx] = prog_base[word_idx] + delta;
                }
                else if (rtype == 1)
                {
                    /* Type 1: load/loadhi pair */
                    unsigned int load_instr;
                    unsigned int loadhi_instr;
                    unsigned int low16;
                    unsigned int high16;
                    unsigned int addr;

                    load_instr = prog_base[word_idx];
                    loadhi_instr = prog_base[word_idx + 1];
                    low16 = (load_instr >> 8) & 0xFFFF;
                    high16 = (loadhi_instr >> 8) & 0xFFFF;
                    addr = (high16 << 16) | low16;
                    addr = addr + delta;
                    prog_base[word_idx] = (load_instr & 0xFF0000FFu) | ((addr & 0xFFFF) << 8);
                    prog_base[word_idx + 1] = (loadhi_instr & 0xFF0000FFu) | (((addr >> 16) & 0xFFFF) << 8);
                }
                else if (rtype == 2)
                {
                    /* Type 2: jump instruction — 27-bit byte address in bits [27:1] */
                    unsigned int instr;
                    unsigned int addr27;

                    instr = prog_base[word_idx];
                    addr27 = (instr >> 1) & 0x7FFFFFFu;
                    addr27 = addr27 + delta;
                    prog_base[word_idx] = (instr & 0xF0000001u) | ((addr27 & 0x7FFFFFFu) << 1);
                }
            }
        }
    }

    kernel_ccache();

    /* Set up process entry */
    p = &proc_table[pid];
    p->state = PROC_READY;
    p->ppid = current_pid;
    p->exit_code = 0;
    p->mem_base = mem_base;
    p->mem_size = mem_size;
    /* Heap starts right after code+data, rounded up to 4-byte boundary */
    p->heap_break = (mem_base + (unsigned int)file_size + 3u) & ~3u;
    p->saved_pc = mem_base;       /* Entry point = start of binary */
    p->saved_hw_sp = 0;          /* Empty HW stack */
    p->fg = 0;
    p->blocked_reason = BLOCK_NONE;
    p->wake_time = 0;
    p->wait_pid = -1;

    /* Initialize software stack: SP at top of process memory */
    {
        int j;
        for (j = 0; j < 16; j++)
            p->saved_regs[j] = 0;
        /* r13 = SP, set to top of allocated region */
        p->saved_regs[13] = mem_base + mem_size - 4;
        /* r14 = FP, same as SP initially */
        p->saved_regs[14] = mem_base + mem_size - 4;
    }

    /* Clear HW stack */
    {
        int j;
        for (j = 0; j < 256; j++)
            p->saved_hw_stack[j] = 0;
    }

    /* Set name from path */
    {
        const char *name_start;
        int len;
        int j;

        name_start = path;
        /* Find last '/' */
        for (j = 0; path[j]; j++)
        {
            if (path[j] == '/')
                name_start = &path[j + 1];
        }
        len = 0;
        while (name_start[len] && len < 31)
        {
            p->name[len] = name_start[len];
            len++;
        }
        p->name[len] = '\0';
    }

    /* Arguments */
    p->argc = argc;
    for (i = 0; i < 16; i++)
    {
        if (i < argc && argv)
            p->argv[i] = argv[i];
        else
            p->argv[i] = (char *)0;
    }

    /* Inherit file descriptors from parent */
    parent = proc_current();
    if (parent)
        fd_inherit(p, parent);
    else
        fd_init_stdio();

    /* Inherit cwd */
    if (parent)
    {
        int j;
        for (j = 0; j < 128; j++)
            p->cwd[j] = parent->cwd[j];
    }

    return pid;
}

void proc_yield(void)
{
    sched_should_yield = 1;
}

void proc_exit(int code)
{
    struct proc *p;
    int i;

    p = proc_current();
    if (!p || p->pid == 0)
    {
        /* Kernel can't exit */
        kernel_panic("kernel exit");
        return;
    }

    /* Close all file descriptors */
    for (i = 0; i < MAX_FDS; i++)
    {
        if (p->fds[i] >= 0)
        {
            vfs_close(p->fds[i]);
            p->fds[i] = -1;
        }
    }

    /* Free memory */
    if (p->mem_base)
    {
        mem_free_region(p->mem_base, p->mem_size);
        p->mem_base = 0;
        p->mem_size = 0;
    }

    p->exit_code = code;
    p->state = PROC_ZOMBIE;

    /* Wake parent if it's waiting on us */
    {
        struct proc *parent;
        parent = proc_by_pid(p->ppid);
        if (parent && parent->state == PROC_BLOCKED
            && parent->blocked_reason == BLOCK_WAITPID
            && (parent->wait_pid == p->pid || parent->wait_pid == -1))
        {
            parent->state = PROC_READY;
            parent->blocked_reason = BLOCK_NONE;
        }
    }

    /* Yield to scheduler */
    sched_should_yield = 1;
}

int proc_waitpid(int pid)
{
    struct proc *p;
    struct proc *target;

    p = proc_current();
    if (!p) return -1;

    if (pid >= 0)
    {
        target = proc_by_pid(pid);
        if (!target) return -1;

        if (target->state == PROC_ZOMBIE)
        {
            int code;
            code = target->exit_code;
            target->state = PROC_FREE;
            return code;
        }

        /* Block until child exits */
        p->state = PROC_BLOCKED;
        p->blocked_reason = BLOCK_WAITPID;
        p->wait_pid = pid;
        sched_should_yield = 1;
        return 0; /* Will be resumed when child exits */
    }

    /* pid == -1: wait for any child */
    {
        int i;
        for (i = 1; i < MAX_PROCS; i++)
        {
            if (proc_table[i].ppid == p->pid
                && proc_table[i].state == PROC_ZOMBIE)
            {
                int code;
                code = proc_table[i].exit_code;
                proc_table[i].state = PROC_FREE;
                return code;
            }
        }
    }

    /* No zombie children — block */
    p->state = PROC_BLOCKED;
    p->blocked_reason = BLOCK_WAITPID;
    p->wait_pid = -1;
    sched_should_yield = 1;
    return 0;
}

void proc_sleep_ms(unsigned int ms)
{
    struct proc *p;
    p = proc_current();
    if (!p) return;

    p->state = PROC_BLOCKED;
    p->blocked_reason = BLOCK_SLEEP;
    p->wake_time = get_micros() + (ms * 1000);
    sched_should_yield = 1;
}
