/*
 * touch — create empty file or update timestamp
 *
 * Usage: touch <file> [file ...]
 * Creates the file if it does not exist.
 */

#include <syscall.h>

int main(void)
{
    int argc;
    char **argv;
    int i;
    int fd;
    int ret;

    argc = sys_argc();
    argv = sys_argv();

    if (argc < 2)
    {
        sys_putstr("usage: touch <file> [file ...]\n");
        return 1;
    }

    ret = 0;
    for (i = 1; i < argc; i++)
    {
        fd = sys_open(argv[i], O_WRONLY | O_CREAT);
        if (fd < 0)
        {
            sys_putstr("touch: cannot create '");
            sys_putstr(argv[i]);
            sys_putstr("'\n");
            ret = 1;
        }
        else
        {
            sys_close(fd);
        }
    }
    return ret;
}
