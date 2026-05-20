/*
 * dev_uart_mirror.c — /dev/uart-mirror virtual device.
 *
 * Provides a simple read/write interface to toggle UART mirroring.
 * Read: returns "0\n" or "1\n".
 * Write: echo 0 > /dev/uart-mirror (disable), echo 1 > /dev/uart-mirror (enable).
 */
#include "kernel.h"

/* ---- Read side ---- */

static int mirror_read(struct open_file *f, void *buf, int count)
{
    char *dst;
    const char *val;

    if (count < 1) return 0;

    dst = (char *)buf;
    val = uart_mirror_get() ? "1\n" : "0\n";

    dst[0] = val[0];
    if (count >= 2) {
        dst[1] = val[1];
        return 2;
    }
    return 1;
}

/* ---- Write side ---- */

static int mirror_write(struct open_file *f, const void *buf, int count)
{
    const char *p;
    int enabled;

    if (count < 1) return 0;

    p = (const char *)buf;
    enabled = (p[0] == '1') ? 1 : 0;
    uart_mirror_set(enabled);

    return count;
}

/* ---- Close ---- */

static int mirror_close(struct open_file *f)
{
    return 0;
}

/* ---- File operations vtable ---- */

static struct file_ops mirror_ops = {
    mirror_read,
    mirror_write,
    0,          /* lseek — not meaningful */
    mirror_close,
    0          /* ioctl — not needed */
};

/* ---- Open callback ---- */

static int mirror_open(const char *path, int flags, struct open_file *f)
{
    f->ops = &mirror_ops;
    return 0;
}

/* ---- Registration ---- */

void dev_uart_mirror_init(void)
{
    vfs_register_device("/dev/uart-mirror", &mirror_ops, mirror_open);
}
