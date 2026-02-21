//
// uart library implementation.
//

#include "libs/kernel/io/uart.h"

// Hardware register addresses
#define UART_TX_ADDR  0x7000000
#define UART_RX_ADDR  0x7000001

// ============================================================
// RX Ring Buffer
// ============================================================

// Ring buffer structure
static volatile char rx_buffer[UART_RX_BUFFER_SIZE];
static volatile int rx_head = 0; // Write position (ISR writes here)
static volatile int rx_tail = 0; // Read position (user reads here)
static volatile int rx_overflow_flag = 0; // Set if data was dropped

// Helper: Get number of bytes in buffer
static int rx_count()
{
    int head = rx_head;
    int tail = rx_tail;
    if (head >= tail) {
        return head - tail;
    }
    return UART_RX_BUFFER_SIZE - tail + head;
}

// ============================================================
// Initialization
// ============================================================

void uart_init()
{
    rx_head = 0;
    rx_tail = 0;
    rx_overflow_flag = 0;
}

// ============================================================
// Transmit Functions
// ============================================================

void uart_putchar(char c)
{
    int *tx_reg = (volatile int *)UART_TX_ADDR;
    *tx_reg = (int)c;
}

// uart puts
void uart_puts(char *str)
{
    if (str == (char *)0) {
        return;
    }
    
    while (*str != '\0') {
        uart_putchar(*str);
        str++;
    }
}

// uart putint
void uart_putint(int value)
{
    char buffer[12];
    itoa(value, buffer, 10);
    uart_puts(buffer);
}

// uart puthex
void uart_puthex(unsigned int value, int prefix)
{
    char buffer[9];
    if (prefix) {
        uart_puts("0x");
    }
    itoa(value, buffer, 16);
    uart_puts(buffer);
}

// uart write
void uart_write(char *buf, unsigned int len)
{
    unsigned int i;
    
    if (buf == (char *)0) {
        return;
    }
    
    for (i = 0; i < len; i++) {
        uart_putchar(buf[i]);
    }
}

// ============================================================
// Receive Functions
// ============================================================

void uart_isr_handler()
{
    volatile int *rx_reg = (volatile int *)UART_RX_ADDR;
    char byte;
    int next_head;
    
    // Read byte from hardware
    byte = (char)(*rx_reg);
    
    // Calculate next head position
    next_head = (rx_head + 1) % UART_RX_BUFFER_SIZE;
    
    // Check for overflow (buffer full)
    if (next_head == rx_tail) {
        // Buffer full - drop the byte and set overflow flag
        rx_overflow_flag = 1;
        return;
    }
    
    // Store byte in buffer
    rx_buffer[rx_head] = byte;
    rx_head = next_head;
}

// uart available
int uart_available()
{
    return rx_count();
}

// uart read
int uart_read()
{
    char byte;
    
    // Check if buffer empty
    if (rx_head == rx_tail) {
        return -1;
    }
    
    // Read byte from buffer
    byte = rx_buffer[rx_tail];
    rx_tail = (rx_tail + 1) % UART_RX_BUFFER_SIZE;
    
    return (int)(unsigned char)byte;
}

// uart peek
int uart_peek()
{
    // Check if buffer empty
    if (rx_head == rx_tail) {
        return -1;
    }
    
    // Return byte without removing
    return (int)(unsigned char)rx_buffer[rx_tail];
}

// uart read bytes
int uart_read_bytes(char *buf, int len)
{
    int count = 0;
    int byte;
    
    if (buf == (char *)0 || len <= 0) {
        return 0;
    }
    
    while (count < len) {
        byte = uart_read();
        if (byte < 0) {
            break; // No more data
        }
        buf[count] = (char)byte;
        count++;
    }
    
    return count;
}

// uart read until
int uart_read_until(char *buf, int len, char terminator)
{
    int count = 0;
    int byte;
    
    if (buf == (char *)0 || len <= 0) {
        return 0;
    }
    
    while (count < len) {
        byte = uart_read();
        if (byte < 0) {
            break; // No more data
        }
        buf[count] = (char)byte;
        count++;
        if ((char)byte == terminator) {
            break; // Found terminator
        }
    }
    
    return count;
}

// uart read line
int uart_read_line(char *buf, int len)
{
    return uart_read_until(buf, len, '\n');
}

// uart flush rx
void uart_flush_rx()
{
    rx_head = 0;
    rx_tail = 0;
    rx_overflow_flag = 0;
}

// uart rx overflow
int uart_rx_overflow()
{
    int flag = rx_overflow_flag;
    rx_overflow_flag = 0; // Clear on read
    return flag;
}
