/*
 * dev.h — device driver interfaces.
 *
 * Each /dev/* device implements a file_ops vtable and registers
 * itself with the VFS at boot via dev_init().
 */
#ifndef KERNEL_DEV_H
#define KERNEL_DEV_H

/* Initialize and register all built-in devices.
 * Called from kernel_init() after vfs_init(). */
void dev_init(void);

/* Individual device init functions (called by dev_init) */
void dev_tty_init(void);
void dev_null_init(void);
void dev_pixpal_init(void);
void dev_uart_init(void);
void dev_random_init(void);
void dev_proc_init(void);
void dev_fb_init(void);
void dev_uart_mirror_init(void);

/* UART mirror shared accessors (used by /dev/tty ioctl + /dev/uart-mirror) */
int  uart_mirror_get(void);
void uart_mirror_set(int enabled);

#endif /* KERNEL_DEV_H */
