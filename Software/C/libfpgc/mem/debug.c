#include "uart.h"
#include "debug.h"
#include <stdio.h>

void
debug_mem_dump(unsigned int *start, unsigned int length)
{
    unsigned int i;
    char buf[12];

    for (i = 0; i < length; i++) {
        snprintf(buf, sizeof(buf), "0x%08x", start[i]);
        uart_puts(buf);
        uart_putchar(' ');
        if ((i + 1) % 8 == 0)
            uart_putchar('\n');
    }
    uart_putchar('\n');
}
