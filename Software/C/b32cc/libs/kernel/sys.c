//
// sys library implementation.
//

#include "libs/kernel/sys.h"

#define BOOT_MODE_ADDR 0x1C000064
#define USER_LED_ADDR 0x1C00006C

// Return the current interrupt identifier.
int get_int_id()
{
  int retval = 0;
  asm(
      "readintid r11      ; r11 = interrupt ID"
      "write -4 r14 r11   ; Write to stack for return");
  return retval;
}

// Return the current boot mode register value.
int get_boot_mode()
{
  int *boot_mode_ptr = (int *)BOOT_MODE_ADDR;
  return *boot_mode_ptr;
}

// Set the user LED state.
void set_user_led(int on)
{
  int *user_led_ptr = (int *)USER_LED_ADDR;
  *user_led_ptr = on;
}
