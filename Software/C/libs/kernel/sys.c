//
// sys library implementation.
//

#include "libs/kernel/sys.h"

#define BOOT_MODE_ADDR 0x7000019
#define MICROS_ADDR 0x700001A
#define USER_LED_ADDR 0x700001B

// Return the current interrupt identifier.
int get_int_id()
{
  int retval = 0;
  asm(
      "readintid r11      ; r11 = interrupt ID"
      "write -1 r14 r11   ; Write to stack for return");
  return retval;
}

// Return the current boot mode register value.
int get_boot_mode()
{
  int *boot_mode_ptr = (int *)BOOT_MODE_ADDR;
  return *boot_mode_ptr;
}

// Return the current microsecond counter value.
unsigned int get_micros()
{
  unsigned int *micros_ptr = (unsigned int *)MICROS_ADDR;
  return *micros_ptr;
}

// Set the user LED state.
void set_user_led(int on)
{
  int *user_led_ptr = (int *)USER_LED_ADDR;
  *user_led_ptr = on;
}
