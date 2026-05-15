/*
 * mv — move/rename file
 *
 * Usage: mv <source> <dest>
 */

#include <syscall.h>

int main(void)
{
    int argc;
    char **argv;

    argc = sys_argc();
    argv = sys_argv();

    if (argc != 3)
    {
        sys_putstr("usage: mv <source> <dest>\n");
        return 1;
    }

    if (sys_rename(argv[1], argv[2]) < 0)
    {
        sys_putstr("mv: cannot rename '");
        sys_putstr(argv[1]);
        sys_putstr("' to '");
        sys_putstr(argv[2]);
        sys_putstr("'\n");
        return 1;
    }

    return 0;
}
