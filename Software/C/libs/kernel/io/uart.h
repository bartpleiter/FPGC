#ifndef UART_H
#define UART_H

// UART library for serial communication
// Provides functions for transmitting and receiving data over UART.
// TX: Direct output (no buffering)
// RX: Ring buffer with interrupt-driven reception
// Usage in interrupt handler:
// void interrupt() {
// int id = get_int_id();
// if (id == INTID_UART) {
// uart_isr_handler();
// }
// }
// Arduino-like API:
// uart_init();              // Initialize (call once)
// while (!uart_available()) {}  // Wait for data
// char c = uart_read();     // Read one byte

// RX buffer size - must be power of 2 for efficient modulo
#define UART_RX_BUFFER_SIZE 64

// ============================================================
// Initialization
// ============================================================

// Initialize the UART subsystem.
// Clears the RX buffer. Call once at program startup.
void uart_init();

// ============================================================
// Transmit Functions (no buffering, direct output)
// ============================================================

// Output a single character.
void uart_putchar(char c);

// Output a null-terminated string.
void uart_puts(char *str);

// Output a signed integer as decimal string.
void uart_putint(int value);

// Output an unsigned integer as hexadecimal string.
void uart_puthex(unsigned int value, int prefix);

// Output a buffer of specified length.
void uart_write(char *buf, unsigned int len);

// ============================================================
// Receive Functions (ring buffer, non-blocking)
// ============================================================

// UART ISR handler. MUST be called from interrupt handler
// when INTID_UART is received.
// Reads the received byte from hardware and stores it in
// the ring buffer.
void uart_isr_handler();

// Get number of bytes available in RX buffer.
int uart_available();

// Read one byte from RX buffer (non-blocking).
int uart_read();

// Peek at next byte without removing from buffer.
int uart_peek();

// Read multiple bytes from RX buffer (non-blocking).
// Reads up to 'len' bytes, returns immediately if fewer available.
int uart_read_bytes(char *buf, int len);

// Read bytes until terminator character or buffer full (non-blocking).
// The terminator is included in the buffer if found.
int uart_read_until(char *buf, int len, char terminator);

// Read a line from RX buffer (until '\n' or buffer full).
// Newline is included if found. Buffer is NOT null-terminated.
int uart_read_line(char *buf, int len);

// Clear the RX buffer, discarding all received data.
void uart_flush_rx();

// Check if RX buffer is full.
// When full, new incoming bytes will be dropped.
int uart_rx_overflow();

#endif // UART_H
