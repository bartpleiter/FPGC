//
// debug library implementation.
//

#include "libs/kernel/mem/debug.h"

// Dump a memory range as hexadecimal words over UART.
void debug_mem_dump(unsigned int *start, unsigned int length)
{
  unsigned int i;

  for (i = 0; i < length; i++)
  {
    uart_puthex(start[i], 1);
    uart_putchar(' ');
    if ((i + 1) % 8 == 0)
    {
      uart_putchar('\n');
    }
  }
  uart_putchar('\n');
}
