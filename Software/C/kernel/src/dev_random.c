/*
 * dev_random.c — /dev/random device driver.
 *
 * Reads produce pseudo-random bytes using a Galois LFSR seeded
 * from the hardware microsecond timer. Writes are discarded.
 */
#include "kernel.h"

static unsigned int random_state;

static int random_read(struct open_file *f, void *buf, int count)
{
    unsigned int s;
    char *p;
    int i;
    unsigned int bit;

    s = random_state;
    if (s == 0) s = 0xACE1u;
    p = (char *)buf;

    for (i = 0; i < count; i++)
    {
        /* 8 LFSR steps per byte */
        int b;
        unsigned int byte_val;
        byte_val = 0;
        for (b = 0; b < 8; b++)
        {
            bit = s & 1u;
            s >>= 1;
            if (bit)
                s ^= 0xB400u; /* taps at 15,13,12,10 */
            byte_val = (byte_val >> 1) | (bit << 7);
        }
        p[i] = (char)byte_val;
    }
    random_state = s;
    return count;
}

static int random_write(struct open_file *f, const void *buf, int count)
{
    return count; /* discard */
}

static int random_close(struct open_file *f)
{
    return 0;
}

static struct file_ops random_ops = {
    random_read,
    random_write,
    0, /* no lseek */
    random_close,
    0  /* no ioctl */
};

static int random_open(const char *path, int flags, struct open_file *f)
{
    return 0;
}

void dev_random_init(void)
{
    /* Seed from hardware timer for entropy */
    random_state = get_micros();
    if (random_state == 0) random_state = 0xACE1u;
    vfs_register_device("/dev/random", &random_ops, random_open);
}
