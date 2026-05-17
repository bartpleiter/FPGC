/*
 * kill — terminate a process by PID
 *
 * Usage: kill <pid>
 */

#include <syscall.h>
#include <stdlib.h>

int main(void)
{
    int argc;
    char **argv;
    int pid;

    argc = sys_argc();
    argv = sys_argv();

    if (argc != 2)
    {
        sys_putstr("usage: kill <pid>\n");
        return 1;
    }

    pid = atoi(argv[1]);
    if (pid <= 0)
    {
        sys_putstr("kill: invalid pid\n");
        return 1;
    }

    if (sys_kill(pid) < 0)
    {
        sys_putstr("kill: no such process\n");
        return 1;
    }

    sys_putstr("killed pid ");
    sys_putstr(argv[1]);
    sys_putc('\n');
    return 0;
}
