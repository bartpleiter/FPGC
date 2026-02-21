//
// BDOS main module and entry point.
//

#include "BDOS/bdos.h"

// Include BDOS code modules
#include "BDOS/init.c"
#include "BDOS/hid.c"
#include "BDOS/fs.c"
#include "BDOS/eth.c"
#include "BDOS/shell_cmds.c"
#include "BDOS/shell.c"

// Print panic message to terminal and UART
void bdos_panic(char* msg)
{
  term_set_palette(PALETTE_WHITE_ON_RED);
  term_puts("BDOS PANIC:\n");
  term_puts(msg);
  term_puts("\n\nSystem halted.\n");

  uart_puts("BDOS PANIC:\n");
  uart_puts(msg);
  uart_puts("\n\nSystem halted.\n");

  asm("halt");
}

// Main BDOS control loop after initialization
void bdos_loop()
{
  while (1)
  {
    bdos_usb_keyboard_main_loop();
    bdos_fnp_poll();
    bdos_shell_tick();
  }
}

// Entry point from bootloader
int main()
{
  bdos_init();
  bdos_fs_boot_init();
  bdos_shell_init();
  bdos_loop();

  // Should not reach here in normal operation
  // Return value gets printed over UART
  return 0x42;
}

// Interrupt handler
void interrupt()
{
  int int_id;

  int_id = get_int_id();
  switch (int_id)
  {
    case INTID_UART:
      break;
    case INTID_TIMER0:
      break;
    case INTID_TIMER1:
      // Used for USB keyboard polling
      timer_isr_handler(TIMER_1);
      break;
    case INTID_TIMER2:
      // Used for delay()
      timer_isr_handler(TIMER_2);
      break;
    case INTID_FRAME_DRAWN:
      break;
    default:
      break;
  }
}
