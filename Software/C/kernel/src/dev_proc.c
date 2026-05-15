/*
 * dev_proc.c — /proc/ virtual filesystem device.
 *
 * Provides read-only virtual files with system information:
 *   /proc/uptime  — system uptime in seconds
 *   /proc/meminfo — memory usage summary
 *   /proc/ps      — process table dump
 */
#include "kernel.h"

/* Proc file types (stored in f->private) */
#define PROC_FILE_UPTIME  0
#define PROC_FILE_MEMINFO 1
#define PROC_FILE_PS      2

/* ---- Integer formatting helpers ---- */

static int proc_itoa(char *buf, unsigned int val)
{
    char tmp[12];
    int len;
    int i;

    len = 0;
    if (val == 0)
    {
        buf[0] = '0';
        return 1;
    }
    while (val > 0)
    {
        tmp[len++] = '0' + (val % 10);
        val /= 10;
    }
    for (i = 0; i < len; i++)
        buf[i] = tmp[len - 1 - i];
    return len;
}

static int proc_strcpy(char *dst, const char *src)
{
    int i;
    i = 0;
    while (src[i])
    {
        dst[i] = src[i];
        i++;
    }
    return i;
}

/* ---- Content generators ---- */

static int gen_uptime(char *buf, int bufsize)
{
    unsigned int us;
    unsigned int secs;
    int len;

    us = get_micros();
    secs = us / 1000000u;
    len = 0;
    len += proc_itoa(buf + len, secs);
    len += proc_strcpy(buf + len, " seconds\n");
    return len;
}

static int gen_meminfo(char *buf, int bufsize)
{
    unsigned int free_bytes;
    int len;

    free_bytes = mem_free_total();
    len = 0;
    len += proc_strcpy(buf + len, "Free: ");
    len += proc_itoa(buf + len, free_bytes);
    len += proc_strcpy(buf + len, " bytes\n");
    return len;
}

static int gen_ps(char *buf, int bufsize)
{
    int i;
    int len;
    struct proc *p;
    const char *state_names[5];

    state_names[0] = "free";
    state_names[1] = "run ";
    state_names[2] = "rdy ";
    state_names[3] = "blk ";
    state_names[4] = "zomb";

    len = 0;
    len += proc_strcpy(buf + len, "PID  STATE  NAME\n");

    for (i = 0; i < MAX_PROCS && len < bufsize - 64; i++)
    {
        p = proc_by_pid(i);
        if (!p) continue;

        if (i < 10) buf[len++] = ' ';
        len += proc_itoa(buf + len, (unsigned int)i);
        len += proc_strcpy(buf + len, "   ");
        if (p->state >= 0 && p->state <= 4)
            len += proc_strcpy(buf + len, state_names[p->state]);
        else
            len += proc_strcpy(buf + len, "??? ");
        len += proc_strcpy(buf + len, "   ");
        len += proc_strcpy(buf + len, p->name);
        buf[len++] = '\n';
    }
    return len;
}

/* ---- File operations ---- */

static int proc_read(struct open_file *f, void *buf, int count)
{
    char content[512];
    int len;
    int avail;
    int type;
    int i;

    type = (int)(unsigned int)f->private;

    switch (type)
    {
    case PROC_FILE_UPTIME:
        len = gen_uptime(content, 512);
        break;
    case PROC_FILE_MEMINFO:
        len = gen_meminfo(content, 512);
        break;
    case PROC_FILE_PS:
        len = gen_ps(content, 512);
        break;
    default:
        return -1;
    }

    avail = len - f->pos;
    if (avail <= 0) return 0;
    if (count > avail) count = avail;

    {
        char *dst;
        dst = (char *)buf;
        for (i = 0; i < count; i++)
            dst[i] = content[f->pos + i];
    }
    f->pos += count;
    return count;
}

static int proc_write(struct open_file *f, const void *buf, int count)
{
    return -1; /* read-only */
}

static int proc_lseek(struct open_file *f, int offset, int whence)
{
    if (whence == 0)
        f->pos = offset;
    else
        return -1;
    return f->pos;
}

static int proc_close(struct open_file *f)
{
    return 0;
}

static struct file_ops proc_ops = {
    proc_read,
    proc_write,
    proc_lseek,
    proc_close,
    0 /* no ioctl */
};

/* ---- Open callback ---- */

/* Match the path suffix after "/proc/" to determine file type */
static int proc_streq(const char *a, const char *b)
{
    while (*a && *b)
    {
        if (*a != *b) return 0;
        a++; b++;
    }
    return *a == *b;
}

static int proc_open(const char *path, int flags, struct open_file *f)
{
    const char *name;

    /* Skip "/proc/" prefix */
    name = path + 6;

    if (proc_streq(name, "uptime"))
        f->private = (void *)PROC_FILE_UPTIME;
    else if (proc_streq(name, "meminfo"))
        f->private = (void *)PROC_FILE_MEMINFO;
    else if (proc_streq(name, "ps"))
        f->private = (void *)PROC_FILE_PS;
    else
        return -1; /* unknown proc file */

    f->pos = 0;
    return 0;
}

/* ---- Registration ---- */

void dev_proc_init(void)
{
    vfs_register_device("/proc/", &proc_ops, proc_open);
}
