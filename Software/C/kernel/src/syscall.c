/*
 * syscall.c — Syscall dispatch table.
 *
 * Called from crt0 Syscall entry: syscall_dispatch(num, a1, a2, a3)
 * where num=r4, a1=r5, a2=r6, a3=r7.
 */
#include "kernel.h"

int syscall_dispatch(int num, int a1, int a2, int a3)
{
    struct proc *p;

    switch (num)
    {
    case SYS_EXIT:
        proc_exit(a1);
        return 0;

    case SYS_READ:
    {
        int gfd;
        gfd = fd_to_gfd(a1);
        if (gfd < 0) return -1;
        return vfs_read(gfd, (void *)a2, a3);
    }

    case SYS_WRITE:
    {
        int gfd;
        gfd = fd_to_gfd(a1);
        if (gfd < 0) return -1;
        return vfs_write(gfd, (const void *)a2, a3);
    }

    case SYS_OPEN:
    {
        int gfd;
        int fd;
        gfd = vfs_open((const char *)a1, a2);
        if (gfd < 0) return -1;
        fd = fd_alloc(gfd);
        if (fd < 0) { vfs_close(gfd); return -1; }
        return fd;
    }

    case SYS_CLOSE:
        return fd_close(a1);

    case SYS_LSEEK:
    {
        int gfd;
        gfd = fd_to_gfd(a1);
        if (gfd < 0) return -1;
        return vfs_lseek(gfd, a2, a3);
    }

    case SYS_DUP2:
        return fd_dup2(a1, a2);

    case SYS_PIPE:
        /* TODO: implement pipes */
        return -1;

    case SYS_EXEC:
        return proc_spawn((const char *)a1, a2, (char **)a3);

    case SYS_YIELD:
        proc_yield();
        return 0;

    case SYS_WAITPID:
        return proc_waitpid(a1);

    case SYS_GETPID:
        return current_pid;

    case SYS_GETCWD:
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
        return 0;

    case SYS_CHDIR:
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

    case SYS_MKDIR:
        return vfs_mkdir((const char *)a1);

    case SYS_UNLINK:
        return vfs_unlink((const char *)a1);

    case SYS_READDIR:
        return vfs_readdir((const char *)a1, (void *)a2, a3);

    case SYS_STAT:
        return vfs_stat((const char *)a1, (void *)a2);

    case SYS_IOCTL:
    {
        int gfd;
        gfd = fd_to_gfd(a1);
        if (gfd < 0) return -1;
        return vfs_ioctl(gfd, a2, a3);
    }

    case SYS_SBRK:
        /* TODO: extend process memory */
        return -1;

    case SYS_SLEEP:
        proc_sleep_ms((unsigned int)a1);
        return 0;

    case SYS_GET_TIME_US:
        return (int)get_micros();

    case SYS_GET_KEY_STATE:
        return (int)hid_key_state;

    case SYS_NET_SEND:
        /* TODO: network send */
        return -1;

    case SYS_NET_RECV:
        /* TODO: network recv */
        return -1;

    case SYS_ARGC:
        p = proc_current();
        if (!p) return 0;
        return p->argc;

    case SYS_ARGV:
        p = proc_current();
        if (!p) return 0;
        if (a1 < 0 || a1 >= p->argc) return 0;
        return (int)p->argv[a1];

    case SYS_FORMAT:
        /* Format is handled via shell built-in, not syscall for now */
        return -1;

    case SYS_SYNC:
        fs_sync_all();
        return 0;

    case SYS_RENAME:
        return vfs_rename((const char *)a1, (const char *)a2);

    case SYS_TRUNCATE:
        /* TODO: truncate */
        return -1;

    default:
        return -1;
    }
}
