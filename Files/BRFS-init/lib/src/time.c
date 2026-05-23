/*
 * time.c — Hardware microsecond counter for user programs.
 */

#include <time.h>

unsigned int get_micros(void)
{
    return (unsigned int)__builtin_load(MICROS_ADDR);
}
