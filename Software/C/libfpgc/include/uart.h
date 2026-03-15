/*
 * uart.h — UART serial driver for B32P3/FPGC.
 *
 * TX: Direct unbuffered output via memory-mapped register.
 * RX: Interrupt-driven ring buffer.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef FPGC_UART_H
#define FPGC_UART_H

/* RX ring buffer size (must be power of 2) */
#define UART_RX_BUFFER_SIZE 64

/* Initialization */
void uart_init(void);

/* Transmit */
void uart_putchar(char c);
void uart_puts(const char *str);
void uart_write(const char *buf, unsigned int len);

/* Receive (non-blocking, from ring buffer) */
void uart_isr_handler(void);
int  uart_available(void);
int  uart_read(void);
int  uart_peek(void);
int  uart_read_bytes(char *buf, int len);
int  uart_read_until(char *buf, int len, char terminator);
int  uart_read_line(char *buf, int len);
void uart_flush_rx(void);
int  uart_rx_overflow(void);

#endif /* FPGC_UART_H */
