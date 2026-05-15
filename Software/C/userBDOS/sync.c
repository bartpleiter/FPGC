/*
 * sync — flush all filesystems to disk
 *
 * Usage: sync
 */

#include <syscall.h>

int main(void)
{
    sys_sync();
    return 0;
}
