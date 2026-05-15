/*
 * dev_pixpal.c — /dev/pixpal device driver.
 *
 * 256-entry pixel palette DAC. Each entry is 4 bytes (0x00RRGGBB).
 * Total size: 1024 bytes. Both cursor and I/O length must be 4-byte aligned.
 *
 * lseek sets byte cursor; write auto-increments one 4-byte entry at a time.
 */
#include "kernel.h"

#define PIXPAL_ENTRIES    256
#define PIXPAL_SIZE_BYTES (PIXPAL_ENTRIES * 4)

static int pixpal_read(struct open_file *f, void *buf, int count)
{
    int *dst;
    int i;
    int cursor;
    int entries;

    cursor = f->pos;
    if (cursor >= PIXPAL_SIZE_BYTES) return 0; /* EOF */
    if ((cursor & 3) || (count & 3)) return -1; /* alignment */
    if (cursor + count > PIXPAL_SIZE_BYTES)
        count = PIXPAL_SIZE_BYTES - cursor;

    dst = (int *)buf;
    entries = count / 4;
    for (i = 0; i < entries; i++)
        dst[i] = (int)gpu_get_pixel_palette((unsigned int)(cursor / 4 + i));

    f->pos = cursor + count;
    return count;
}

static int pixpal_write(struct open_file *f, const void *buf, int count)
{
    const int *src;
    int i;
    int cursor;
    int entries;

    cursor = f->pos;
    if ((cursor & 3) || (count & 3)) return -1; /* alignment */
    if (cursor + count > PIXPAL_SIZE_BYTES) return -1; /* past end */

    src = (const int *)buf;
    entries = count / 4;
    for (i = 0; i < entries; i++)
        gpu_set_pixel_palette((unsigned int)(cursor / 4 + i),
                              (unsigned int)src[i] & 0x00FFFFFF);

    f->pos = cursor + count;
    return count;
}

static int pixpal_lseek(struct open_file *f, int offset, int whence)
{
    int newpos;

    if (whence == 0)      /* SEEK_SET */
        newpos = offset;
    else if (whence == 1) /* SEEK_CUR */
        newpos = f->pos + offset;
    else if (whence == 2) /* SEEK_END */
        newpos = PIXPAL_SIZE_BYTES + offset;
    else
        return -1;

    if (newpos < 0 || newpos > PIXPAL_SIZE_BYTES)
        return -1;

    f->pos = newpos;
    return newpos;
}

static int pixpal_close(struct open_file *f)
{
    return 0;
}

static struct file_ops pixpal_ops = {
    pixpal_read,
    pixpal_write,
    pixpal_lseek,
    pixpal_close,
    0 /* no ioctl */
};

static int pixpal_open(const char *path, int flags, struct open_file *f)
{
    f->pos = 0;
    return 0;
}

void dev_pixpal_init(void)
{
    vfs_register_device("/dev/pixpal", &pixpal_ops, pixpal_open);
}
