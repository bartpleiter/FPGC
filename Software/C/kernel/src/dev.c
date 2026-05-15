/*
 * dev.c — Device subsystem initialization.
 *
 * Registers all built-in /dev/ devices.
 */
#include "kernel.h"

void dev_init(void)
{
    dev_tty_init();
    dev_null_init();
    dev_pixpal_init();
    /* dev_fb_init() will be added later */
}
