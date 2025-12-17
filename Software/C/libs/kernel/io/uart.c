#include "libs/kernel/io/uart.h"

void uart_send_char(char c)
{
    asm(
        "load32 0x7000000 r11 ; r11 = UART TX register"
        "write 0 r11 r4       ; Write data to UART"
    );
}
