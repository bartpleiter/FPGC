#include "libs/kernel/io/uart.h"

// Output a single character
void uart_putchar(char c)
{
    asm(
        "load32 0x7000000 r11 ; r11 = UART TX register"
        "write 0 r11 r4       ; Write data to UART"
    );
}

// Output a null-terminated string
void uart_puts(char *str) {
    if (str == (char *)0) {
        return;
    }
    
    while (*str != '\0') {
        uart_putchar(*str);
        str++;
    }
}

// Output an integer as a string
void uart_putint(int value)
{
    char buffer[12];
    itoa(value, buffer, 10);
    uart_puts(buffer);
}

// Output an unsigned integer as a hexadecimal string, with optional "0x" prefix
void uart_puthex(unsigned int value, int prefix)
{
    if (prefix) {
        uart_puts("0x");
    }
    char buffer[9];
    itoa(value, buffer, 16);
    uart_puts(buffer);
}

// Output a buffer of specified length
void uart_write(char *buf, unsigned int len) {
    unsigned int i;
    
    if (buf == (char *)0) {
        return;
    }
    
    for (i = 0; i < len; i++) {
        uart_putchar(buf[i]);
    }
}
