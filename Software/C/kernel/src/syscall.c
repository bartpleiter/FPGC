/*
 * syscall.c — BDOS v4 syscall dispatch.
 *
 * Called from crt0 Syscall entry: syscall_dispatch(num, a1, a2, a3)
 * where num=r4, a1=r5, a2=r6, a3=r7.
 *
 * Clean v4 design — POSIX-aligned numbering, no v3 compatibility.
 */
#include "kernel.h"

int syscall_dispatch(int num, int a1, int a2, int a3)
{
    struct proc *p;

    /* Check for Ctrl+C interrupt — force exit if pending */
    if (ctrl_c_pending && current_pid != 0)
    {
        ctrl_c_pending = 0;
        proc_exit(130); /* 128 + SIGINT(2), POSIX convention */
        syscall_exit_to_kernel();
        /* not reached */
    }

    switch (num)
    {
    /* ---- Core process control (1-5) ---- */

    case SYS_EXIT:       /* 1 */
        proc_exit(a1);
        syscall_exit_to_kernel();
        return 0; /* not reached */

    case SYS_YIELD:      /* 2 */
        proc_yield();
        return 0;

    case SYS_EXEC:       /* 3 */
        return proc_spawn((const char *)a1, a2, (char **)a3);

    case SYS_WAITPID:    /* 4 */
        return proc_waitpid(a1);

    case SYS_GETPID:     /* 5 */
        return current_pid;

    /* ---- File I/O (10-15) ---- */

    case SYS_OPEN:       /* 10 */
    {
        int gfd;
        int fd;
        gfd = vfs_open((const char *)a1, a2);
        if (gfd < 0) return -1;
        fd = fd_alloc(gfd);
        if (fd < 0) { vfs_close(gfd); return -1; }
        return fd;
    }

    case SYS_CLOSE:      /* 11 */
        return fd_close(a1);

    case SYS_READ:       /* 12 */
    {
        int gfd;
        gfd = fd_to_gfd(a1);
        if (gfd < 0) return -1;
        return vfs_read(gfd, (void *)a2, a3);
    }

    case SYS_WRITE:      /* 13 */
    {
        int gfd;
        gfd = fd_to_gfd(a1);
        if (gfd < 0) return -1;
        return vfs_write(gfd, (const void *)a2, a3);
    }

    case SYS_LSEEK:      /* 14 */
    {
        int gfd;
        gfd = fd_to_gfd(a1);
        if (gfd < 0) return -1;
        return vfs_lseek(gfd, a2, a3);
    }

    case SYS_DUP2:       /* 15 */
        return fd_dup2(a1, a2);

    /* ---- Filesystem (20-28) ---- */

    case SYS_UNLINK:     /* 20 */
        return vfs_unlink((const char *)a1);

    case SYS_MKDIR:      /* 21 */
        return vfs_mkdir((const char *)a1);

    case SYS_READDIR:    /* 22 */
        return vfs_readdir((const char *)a1, (void *)a2, a3);

    case SYS_RENAME:     /* 23 */
        return vfs_rename((const char *)a1, (const char *)a2);

    case SYS_STAT:       /* 24 */
        return vfs_stat((const char *)a1, (void *)a2);

    case SYS_SYNC:       /* 25 */
        fs_sync_all();
        return 0;

    case SYS_TRUNCATE:   /* 26 */
        return -1; /* Phase 2 */

    case SYS_FORMAT:     /* 27 */
        return -1; /* Phase 2 */

    case SYS_SD_FORMAT:  /* 28 */
        return -1; /* Phase 2 */

    /* ---- Process environment (30-34) ---- */

    case SYS_CHDIR:      /* 30 */
        p = proc_current();
        if (!p) return -1;
        {
            const char *src;
            int i;
            src = (const char *)a1;
            for (i = 0; i < 127 && src[i]; i++)
                p->cwd[i] = src[i];
            p->cwd[i] = '\0';
        }
        return 0;

    case SYS_GETCWD:     /* 31 — getcwd(buf, size): copies cwd into user buffer */
        p = proc_current();
        if (!p) return -1;
        {
            char *dst;
            int i;
            dst = (char *)a1;
            for (i = 0; i < a2 - 1 && p->cwd[i]; i++)
                dst[i] = p->cwd[i];
            dst[i] = '\0';
        }
        return a1; /* returns buf pointer, like POSIX */

    case SYS_ARGC:       /* 32 */
        p = proc_current();
        if (!p) return 0;
        return p->argc;

    case SYS_ARGV:       /* 33 — returns char** (pointer to argv array) */
        p = proc_current();
        if (!p) return 0;
        return (int)p->argv;

    case SYS_SBRK:       /* 34 — sbrk(incr): extend process heap */
        p = proc_current();
        if (!p) return -1;
        {
            unsigned int old_break;
            unsigned int new_break;
            unsigned int limit;
            old_break = p->heap_break;
            new_break = old_break + (unsigned int)a1;
            /* Leave 64 KiB for stack at top of process memory */
            limit = p->mem_base + p->mem_size - (64u * 1024u);
            if (new_break > limit || new_break < p->mem_base)
                return -1;
            p->heap_break = new_break;
        }
        return (int)p->heap_break - a1; /* return old break */

    /* ---- Timing / input (40-42) ---- */

    case SYS_SLEEP:      /* 40 — sleep(ms) */
        delay((unsigned int)a1);
        return 0;

    case SYS_GET_KEY_STATE: /* 41 */
        return (int)hid_key_state;

    case SYS_GET_TIME_US: /* 42 */
        return (int)get_micros();

    /* ---- Networking (50-53) ---- */

    case SYS_NET_SEND:   /* 50 — net_send(buf, len): send raw Ethernet frame */
        enc28j60_packet_send((char *)a1, (int)a2);
        return a2;

    case SYS_NET_RECV:   /* 51 — net_recv(buf, max): receive packet from ring */
        return net_ringbuf_pop((char *)a1, a2);

    case SYS_NET_PACKET_COUNT: /* 52 */
        return net_ringbuf_count();

    case SYS_NET_GET_MAC: /* 53 — net_get_mac(buf6): copy MAC to user buffer */
    {
        char *dst;
        dst = (char *)a1;
        dst[0] = (char)net_mac[0];
        dst[1] = (char)net_mac[1];
        dst[2] = (char)net_mac[2];
        dst[3] = (char)net_mac[3];
        dst[4] = (char)net_mac[4];
        dst[5] = (char)net_mac[5];
        return 0;
    }

    /* ---- IPC (60-61) ---- */

    case SYS_PIPE:       /* 60 */
        return -1; /* Phase 2 */

    case SYS_IOCTL:      /* 61 */
    {
        int gfd;
        gfd = fd_to_gfd(a1);
        if (gfd < 0) return -1;
        return vfs_ioctl(gfd, a2, a3);
    }

    default:
        return -1;
    }
}
