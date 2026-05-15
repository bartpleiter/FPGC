/*
 * mkdir — create directories
 *
 * Usage: mkdir [-p] <path> [path ...]
 * -p: no error if existing
 */

#include <syscall.h>

int main(void)
{
    int argc;
    char **argv;
    int i;
    int ret;
    int pflag;

    argc = sys_argc();
    argv = sys_argv();

    pflag = 0;
    for (i = 1; i < argc; i++)
    {
        if (argv[i][0] == '-' && argv[i][1] == 'p' && argv[i][2] == '\0')
            pflag = 1;
    }

    if (argc < 2 || (pflag && argc < 3))
    {
        sys_putstr("usage: mkdir [-p] <path> [path ...]\n");
        return 1;
    }

    ret = 0;
    for (i = 1; i < argc; i++)
    {
        if (argv[i][0] == '-') continue;
        if (sys_mkdir(argv[i]) < 0)
        {
            if (!pflag)
            {
                sys_putstr("mkdir: cannot create '");
                sys_putstr(argv[i]);
                sys_putstr("'\n");
                ret = 1;
            }
        }
    }
    return ret;
}
