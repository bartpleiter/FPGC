/*
 * init.c — /bin/init for BDOS v4.
 *
 * First user process (PID 1). Spawns /bin/sh in a loop.
 * If sh exits or crashes, respawns it.
 */
#include <syscall.h>

int main(void)
{
    int pid;
    int status;

    sys_putstr("init: starting\n");

    while (1)
    {
        pid = sys_exec("/bin/sh", 0, (const char **)0);
        if (pid < 0)
        {
            sys_putstr("init: failed to start /bin/sh\n");
            sys_sleep(1000);
            continue;
        }
        status = sys_waitpid(pid);
        sys_putstr("init: sh exited (");
        /* Print exit code as single digit or ? */
        if (status >= 0 && status <= 9)
            sys_putc('0' + status);
        else
            sys_putc('?');
        sys_putstr("), respawning\n");
    }
}
