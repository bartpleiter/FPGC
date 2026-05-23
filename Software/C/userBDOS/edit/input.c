#include <syscall.h>
#include "input.h"

static int tty_fd = -1;

int input_init(void)
{
    tty_fd = sys_tty_open_raw(0);
    return tty_fd;
}

void input_close(void)
{
    if (tty_fd >= 0) {
        sys_close(tty_fd);
        tty_fd = -1;
    }
}

int input_read_key(void)
{
    if (tty_fd < 0) return -1;
    return sys_tty_event_read(tty_fd, 1);
}
