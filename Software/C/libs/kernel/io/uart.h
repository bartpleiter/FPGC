#ifndef UART_H
#define UART_H

/*
 * UART library for serial communication
 * Provides functions for transmitting and receiving data over UART.
 * 
 * TX: Direct output (no buffering)
 * RX: Ring buffer with interrupt-driven reception
 * 
 * Usage in interrupt handler:
 *   void interrupt() {
 *       int id = get_int_id();
 *       if (id == INTID_UART) {
 *           uart_isr_handler();
 *       }
 *   }
 * 
 * Arduino-like API:
 *   uart_init();              // Initialize (call once)
 *   while (!uart_available()) {}  // Wait for data
 *   char c = uart_read();     // Read one byte
 */

/* RX buffer size - must be power of 2 for efficient modulo */
#define UART_RX_BUFFER_SIZE 64

/*
 * ============================================================
 * Initialization
 * ============================================================
 */

/**
 * Initialize the UART subsystem.
 * Clears the RX buffer. Call once at program startup.
 */
void uart_init();

/*
 * ============================================================
 * Transmit Functions (no buffering, direct output)
 * ============================================================
 */

/**
 * Output a single character.
 * @param c Character to transmit
 */
void uart_putchar(char c);

/**
 * Output a null-terminated string.
 * @param str String to transmit (NULL safe)
 */
void uart_puts(char *str);

/**
 * Output a signed integer as decimal string.
 * @param value Integer value to transmit
 */
void uart_putint(int value);

/**
 * Output an unsigned integer as hexadecimal string.
 * @param value Unsigned integer value
 * @param prefix Non-zero to include "0x" prefix
 */
void uart_puthex(unsigned int value, int prefix);

/**
 * Output a buffer of specified length.
 * @param buf Buffer to transmit (NULL safe)
 * @param len Number of bytes to transmit
 */
void uart_write(char *buf, unsigned int len);

/*
 * ============================================================
 * Receive Functions (ring buffer, non-blocking)
 * ============================================================
 */

/**
 * UART ISR handler. MUST be called from interrupt handler
 * when INTID_UART is received.
 * 
 * Reads the received byte from hardware and stores it in
 * the ring buffer.
 */
void uart_isr_handler();

/**
 * Get number of bytes available in RX buffer.
 * @return Number of bytes available to read
 */
int uart_available();

/**
 * Read one byte from RX buffer (non-blocking).
 * @return Byte read (0-255), or -1 if buffer empty
 */
int uart_read();

/**
 * Peek at next byte without removing from buffer.
 * @return Next byte (0-255), or -1 if buffer empty
 */
int uart_peek();

/**
 * Read multiple bytes from RX buffer (non-blocking).
 * Reads up to 'len' bytes, returns immediately if fewer available.
 * @param buf Buffer to store received data
 * @param len Maximum number of bytes to read
 * @return Number of bytes actually read
 */
int uart_read_bytes(char *buf, int len);

/**
 * Read bytes until terminator character or buffer full (non-blocking).
 * The terminator is included in the buffer if found.
 * @param buf Buffer to store received data
 * @param len Maximum number of bytes to read
 * @param terminator Character to stop reading at
 * @return Number of bytes read (including terminator if found)
 */
int uart_read_until(char *buf, int len, char terminator);

/**
 * Read a line from RX buffer (until '\n' or buffer full).
 * Newline is included if found. Buffer is NOT null-terminated.
 * @param buf Buffer to store the line
 * @param len Maximum buffer size
 * @return Number of bytes read
 */
int uart_read_line(char *buf, int len);

/**
 * Clear the RX buffer, discarding all received data.
 */
void uart_flush_rx();

/**
 * Check if RX buffer is full.
 * When full, new incoming bytes will be dropped.
 * @return Non-zero if buffer is full
 */
int uart_rx_overflow();

#endif // UART_H
