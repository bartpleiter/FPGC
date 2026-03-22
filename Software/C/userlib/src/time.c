/*
 * time.c — Hardware microsecond counter for user programs.
 */

#include <time.h>
#include <hwio.h>

unsigned int get_micros(void)
{
    return (unsigned int)hwio_read(MICROS_ADDR);
}
