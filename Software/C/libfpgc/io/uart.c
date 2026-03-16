#include "fpgc.h"
#include "uart.h"
#include <stddef.h>
#include <stdio.h>

/* RX ring buffer */
static char rx_buffer[UART_RX_BUFFER_SIZE];
static int rx_head = 0;
static int rx_tail = 0;
static int rx_overflow_flag = 0;

static int
rx_count(void)
{
    int head = rx_head;
    int tail = rx_tail;
    if (head >= tail)
        return head - tail;
    return UART_RX_BUFFER_SIZE - tail + head;
}

void
uart_init(void)
{
    rx_head = 0;
    rx_tail = 0;
    rx_overflow_flag = 0;
}

void
uart_putchar(char c)
{
    hwio_write(FPGC_UART_TX, (int)(unsigned char)c);
}

void
uart_puts(const char *str)
{
    if (!str)
        return;
    while (*str)
        uart_putchar(*str++);
}

void
uart_write(const char *buf, unsigned int len)
{
    unsigned int i;
    if (!buf)
        return;
    for (i = 0; i < len; i++)
        uart_putchar(buf[i]);
}

void
uart_putint(int value)
{
    char buffer[12];
    snprintf(buffer, sizeof(buffer), "%d", value);
    uart_puts(buffer);
}

void
uart_puthex(unsigned int value, int prefix)
{
    char buffer[9];
    if (prefix)
        uart_puts("0x");
    snprintf(buffer, sizeof(buffer), "%x", value);
    uart_puts(buffer);
}

void
uart_isr_handler(void)
{
    char byte;
    int next_head;

    byte = (char)hwio_read(FPGC_UART_RX);
    next_head = (rx_head + 1) % UART_RX_BUFFER_SIZE;

    if (next_head == rx_tail) {
        rx_overflow_flag = 1;
        return;
    }

    rx_buffer[rx_head] = byte;
    rx_head = next_head;
}

int
uart_available(void)
{
    return rx_count();
}

int
uart_read(void)
{
    char byte;
    if (rx_head == rx_tail)
        return -1;
    byte = rx_buffer[rx_tail];
    rx_tail = (rx_tail + 1) % UART_RX_BUFFER_SIZE;
    return (int)(unsigned char)byte;
}

int
uart_peek(void)
{
    if (rx_head == rx_tail)
        return -1;
    return (int)(unsigned char)rx_buffer[rx_tail];
}

int
uart_read_bytes(char *buf, int len)
{
    int count = 0;
    int byte;
    if (!buf || len <= 0)
        return 0;
    while (count < len) {
        byte = uart_read();
        if (byte < 0)
            break;
        buf[count++] = (char)byte;
    }
    return count;
}

int
uart_read_until(char *buf, int len, char terminator)
{
    int count = 0;
    int byte;
    if (!buf || len <= 0)
        return 0;
    while (count < len) {
        byte = uart_read();
        if (byte < 0)
            break;
        buf[count++] = (char)byte;
        if ((char)byte == terminator)
            break;
    }
    return count;
}

int
uart_read_line(char *buf, int len)
{
    return uart_read_until(buf, len, '\n');
}

void
uart_flush_rx(void)
{
    rx_head = 0;
    rx_tail = 0;
    rx_overflow_flag = 0;
}

int
uart_rx_overflow(void)
{
    int flag = rx_overflow_flag;
    rx_overflow_flag = 0;
    return flag;
}
