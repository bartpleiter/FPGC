#ifndef UART_H
#define UART_H

/*
 * UART library for UART communication
 * Provides functions to interact with UART.
 */

// Output a single character
void uart_putchar(char c);

// Output a null-terminated string
void uart_puts(char *str);

// Output an integer as a string
void uart_putint(int value);

// Output an unsigned integer as a hexadecimal string, with optional "0x" prefix
void uart_puthex(unsigned int value, int prefix);

// Output a buffer of specified length
void uart_write(char *buf, unsigned int len);

#endif // UART_H
