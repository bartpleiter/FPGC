/*
 * mkdir — create directories
 *
 * Usage: mkdir <path> [path ...]
 */

#include <syscall.h>

int main(void)
{
    int argc;
    char **argv;
    int i;
    int ret;

    argc = sys_argc();
    argv = sys_argv();

    if (argc < 2)
    {
        sys_putstr("usage: mkdir <path> [path ...]\n");
        return 1;
    }

    ret = 0;
    for (i = 1; i < argc; i++)
    {
        if (sys_mkdir(argv[i]) < 0)
        {
            sys_putstr("mkdir: cannot create '");
            sys_putstr(argv[i]);
            sys_putstr("'\n");
            ret = 1;
        }
    }
    return ret;
}
