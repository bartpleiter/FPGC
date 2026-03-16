#include "fpgc.h"
#include "sys.h"

int
get_boot_mode(void)
{
    return hwio_read(FPGC_BOOT_MODE);
}

void
set_user_led(int on)
{
    hwio_write(FPGC_USER_LED, on);
}

unsigned int
get_micros(void)
{
    return (unsigned int)hwio_read(FPGC_MICROS);
}
