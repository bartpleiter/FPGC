/*
 * dev_uart.c — /dev/uart device driver.
 *
 * Raw serial port access (UART TX/RX).
 * Write sends bytes over UART. Read consumes from UART receive buffer.
 */
#include "kernel.h"

static int uart_dev_read(struct open_file *f, void *buf, int count)
{
    return uart_read_bytes((char *)buf, count);
}

static int uart_dev_write(struct open_file *f, const void *buf, int count)
{
    uart_write((const char *)buf, (unsigned int)count);
    return count;
}

static int uart_dev_close(struct open_file *f)
{
    return 0;
}

static struct file_ops uart_dev_ops = {
    uart_dev_read,
    uart_dev_write,
    0, /* no lseek */
    uart_dev_close,
    0  /* no ioctl */
};

static int uart_dev_open(const char *path, int flags, struct open_file *f)
{
    return 0;
}

void dev_uart_init(void)
{
    vfs_register_device("/dev/uart", &uart_dev_ops, uart_dev_open);
}
