//
// sys library implementation.
//

#include "libs/kernel/sys.h"

#define BOOT_MODE_ADDR  0x7000019
#define MICROS_ADDR     0x700001A
#define USER_LED_ADDR   0x700001B


// Get int id
int get_int_id()
{
    int retval = 0;
    asm(
        "readintid r11      ; r11 = interrupt ID"
        "write -1 r14 r11   ; Write to stack for return"
    );
    return retval;
}

// Get boot mode
int get_boot_mode()
{
    int* boot_mode_ptr = (int*)BOOT_MODE_ADDR;
    return *boot_mode_ptr;
}

// Get micros
unsigned int get_micros()
{
    unsigned int* micros_ptr = (unsigned int*)MICROS_ADDR;
    return *micros_ptr;
}

// Set user led
void set_user_led(int on)
{
    int* user_led_ptr = (int*)USER_LED_ADDR;
    *user_led_ptr = on;
}
