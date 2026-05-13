/*
 * dev_null.c — /dev/null and /dev/zero devices.
 *
 * /dev/null: write succeeds (discards), read returns 0 bytes.
 * /dev/zero: write succeeds (discards), read fills buffer with 0.
 */
#include "kernel.h"

/* ---- /dev/null ---- */

static int null_read(struct open_file *f, void *buf, int count)
{
    return 0; /* EOF */
}

static int null_write(struct open_file *f, const void *buf, int count)
{
    return count; /* Success, discard everything */
}

static int null_close(struct open_file *f)
{
    return 0;
}

static struct file_ops null_ops = {
    null_read,
    null_write,
    0,
    null_close,
    0
};

static int null_open(const char *path, int flags, struct open_file *f)
{
    f->ops = &null_ops;
    return 0;
}

/* ---- /dev/zero ---- */

static int zero_read(struct open_file *f, void *buf, int count)
{
    char *p;
    int i;
    p = (char *)buf;
    for (i = 0; i < count; i++)
        p[i] = 0;
    return count;
}

static struct file_ops zero_ops = {
    zero_read,
    null_write, /* write discards, same as /dev/null */
    0,
    null_close,
    0
};

static int zero_open(const char *path, int flags, struct open_file *f)
{
    f->ops = &zero_ops;
    return 0;
}

/* ---- Registration ---- */

void dev_null_init(void)
{
    vfs_register_device("/dev/null", &null_ops, null_open);
    vfs_register_device("/dev/zero", &zero_ops, zero_open);
}
