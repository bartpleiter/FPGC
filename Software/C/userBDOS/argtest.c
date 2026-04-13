/* argtest.c — debug argc/argv on BDOS */
#include <stdio.h>

int main(int argc, char **argv)
{
    int i;
    printf("argc = %d\n", argc);
    for (i = 0; i < argc && i < 8; i++) {
        if (argv[i])
            printf("argv[%d] = '%s'\n", i, argv[i]);
        else
            printf("argv[%d] = (null)\n", i);
    }
    return 0;
}
