/*
 * dev_tty.c — /dev/tty device driver.
 *
 * Read: returns key events from HID FIFO (with line editing).
 * Write: sends chars to the terminal (libterm) and optionally UART.
 */
#include "kernel.h"

/* ---- Write side ---- */

static int tty_write(struct open_file *f, const void *buf, int count)
{
    const char *p;
    int i;

    p = (const char *)buf;
    for (i = 0; i < count; i++)
    {
        term_putchar(p[i]);
    }
    return count;
}

/* ---- Read side (blocking) ---- */

/* Minimal line-editing buffer */
#define TTY_LINE_MAX 256
static char tty_line_buf[TTY_LINE_MAX];
static int  tty_line_len;
static int  tty_line_pos;     /* read cursor within completed line */
static int  tty_line_ready;   /* 1 = a full line is ready for reading */

/* Build a line from key events. Returns 1 when Enter pressed. */
static int tty_line_edit(void)
{
    int ch;
    while (hid_event_available())
    {
        ch = hid_event_read();
        if (ch < 0) return 0;

        /* Handle ASCII printable + special keys */
        if (ch == '\n' || ch == '\r')
        {
            term_putchar('\n');
            tty_line_buf[tty_line_len] = '\n';
            tty_line_len++;
            tty_line_ready = 1;
            tty_line_pos = 0;
            return 1;
        }
        else if (ch == '\b' || ch == 0x7F)
        {
            /* Backspace */
            if (tty_line_len > 0)
            {
                tty_line_len--;
                term_putchar('\b');
                term_putchar(' ');
                term_putchar('\b');
            }
        }
        else if (ch >= 0x20 && ch < 0x7F)
        {
            if (tty_line_len < TTY_LINE_MAX - 1)
            {
                tty_line_buf[tty_line_len] = (char)ch;
                tty_line_len++;
                term_putchar((char)ch);
            }
        }
        /* Ignore other keys for now */
    }
    return 0;
}

static int tty_read(struct open_file *f, void *buf, int count)
{
    char *dst;
    int copied;
    int avail;

    dst = (char *)buf;
    copied = 0;

    /* If raw mode requested, return whatever HID gives us directly */
    if (f->flags & O_RAW)
    {
        while (copied < count)
        {
            int ch;
            ch = hid_event_read();
            if (ch < 0) break;
            dst[copied] = (char)ch;
            copied++;
        }
        return copied;
    }

    /* Cooked mode: line editing */
    if (tty_line_ready)
    {
        /* Serve from existing line buffer */
        avail = tty_line_len - tty_line_pos;
        if (avail > count) avail = count;
        /* Copy bytes */
        {
            int i;
            for (i = 0; i < avail; i++)
                dst[i] = tty_line_buf[tty_line_pos + i];
        }
        tty_line_pos += avail;
        if (tty_line_pos >= tty_line_len)
        {
            tty_line_ready = 0;
            tty_line_len = 0;
            tty_line_pos = 0;
        }
        return avail;
    }

    /* No line ready — attempt to build one (non-blocking attempt) */
    tty_line_edit();
    if (tty_line_ready)
    {
        avail = tty_line_len - tty_line_pos;
        if (avail > count) avail = count;
        {
            int i;
            for (i = 0; i < avail; i++)
                dst[i] = tty_line_buf[tty_line_pos + i];
        }
        tty_line_pos += avail;
        if (tty_line_pos >= tty_line_len)
        {
            tty_line_ready = 0;
            tty_line_len = 0;
            tty_line_pos = 0;
        }
        return avail;
    }

    return 0; /* No data available yet */
}

/* ---- Close / ioctl ---- */

static int tty_close(struct open_file *f)
{
    return 0;
}

static int tty_ioctl(struct open_file *f, int cmd, int arg)
{
    /* TODO: set raw/cooked mode, etc. */
    return -1;
}

/* ---- File operations vtable ---- */

static struct file_ops tty_ops = {
    tty_read,
    tty_write,
    0,          /* lseek — not meaningful for TTY */
    tty_close,
    tty_ioctl
};

/* ---- Open callback for VFS registration ---- */

static int tty_open(const char *path, int flags, struct open_file *f)
{
    f->ops = &tty_ops;
    return 0;
}

/* ---- Registration ---- */

void dev_tty_init(void)
{
    tty_line_len = 0;
    tty_line_pos = 0;
    tty_line_ready = 0;
    vfs_register_device("/dev/tty", &tty_ops, tty_open);
}
