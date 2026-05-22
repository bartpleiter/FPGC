#include "input.h"
#include <syscall.h>

static int tty_fd = -1;

void input_init(void)
{
    tty_fd = sys_tty_open_raw(0 /* blocking */);
}

int input_read_key(void)
{
    return sys_tty_event_read(tty_fd, 1 /* blocking */);
}

void input_close(void)
{
    if (tty_fd >= 0) sys_close(tty_fd);
    tty_fd = -1;
}
