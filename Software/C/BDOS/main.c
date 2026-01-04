/*
 * BDOS main module and entry point.
 */

#include "BDOS/bdos.h"

// Include BDOS code modules
#include "BDOS/init.c"

void bdos_panic(const char* msg)
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

// Main entry point
int main()
{
  // Initialize BDOS
  bdos_init();

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
      timer_isr_handler(TIMER_1);
      break;
    case INTID_TIMER2:
      timer_isr_handler(TIMER_2);
      break;
    case INTID_FRAME_DRAWN:
      break;
    default:
      break;
  }
}
