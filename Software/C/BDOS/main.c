/*
 * BDOS main module and entry point.
 */

#include "BDOS/bdos.h"

// Include BDOS code modules
#include "BDOS/init.c"
#include "BDOS/hid.c"
#include "BDOS/fs.c"
#include "BDOS/eth.c"
#include "BDOS/shell_cmds.c"
#include "BDOS/shell.c"

void bdos_panic(char* msg)
{
  // Print panic message to terminal and UART
  term_set_palette(PALETTE_WHITE_ON_RED);
  term_puts("BDOS PANIC:\n");
  term_puts(msg);
  term_puts("\n\nSystem halted.\n");

  uart_puts("BDOS PANIC:\n");
  uart_puts(msg);
  uart_puts("\n\nSystem halted.\n");

  // Halt system
  asm("halt");
}

void bdos_loop()
{
  // Main loop for BDOS after initialization
  while (1)
  {
    // Poll for USB keyboard connection and input
    bdos_usb_keyboard_main_loop();

    // Run shell using keyboard FIFO events
    bdos_shell_tick();
  }
}

// Main entry point
int main()
{
  // Initialize BDOS
  bdos_init();

  // Initialize filesystem
  bdos_fs_boot_init();

  // Start shell
  bdos_shell_init();

  // Enter main loop
  bdos_loop();

  // Return value gets printed over UART
  // Should not reach here in normal operation
  return 0x42;
}

// Interrupt handler
void interrupt()
{
  int int_id = get_int_id();
  switch (int_id)
  {
    case INTID_UART:
      break;
    case INTID_TIMER0:
      timer_isr_handler(TIMER_0);
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
