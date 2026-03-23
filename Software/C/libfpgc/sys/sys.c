#include "fpgc.h"
#include "sys.h"

int
get_boot_mode(void)
{
    return __builtin_load(FPGC_BOOT_MODE);
}

void
set_user_led(int on)
{
    __builtin_store(FPGC_USER_LED, on);
}

unsigned int
get_micros(void)
{
    return (unsigned int)__builtin_load(FPGC_MICROS);
}
