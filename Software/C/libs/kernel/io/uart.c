//
// uart library implementation.
//

#include "libs/kernel/io/uart.h"

// Hardware register addresses
#define UART_TX_ADDR 0x7000000
#define UART_RX_ADDR 0x7000001

// ---- RX Ring Buffer ----
// Ring buffer structure
static volatile char rx_buffer[UART_RX_BUFFER_SIZE];
static volatile int rx_head = 0;          // Write position (ISR writes here)
static volatile int rx_tail = 0;          // Read position (user reads here)
static volatile int rx_overflow_flag = 0; // Set if data was dropped

// Helper: Get number of bytes in buffer
static int rx_count()
{
  int head = rx_head;
  int tail = rx_tail;
  if (head >= tail)
  {
    return head - tail;
  }
  return UART_RX_BUFFER_SIZE - tail + head;
}

// ---- Initialization ----
void uart_init()
{
  rx_head = 0;
  rx_tail = 0;
  rx_overflow_flag = 0;
}

// ---- Transmit Functions ----
void uart_putchar(char c)
{
  int *tx_reg = (volatile int *)UART_TX_ADDR;
  *tx_reg = (int)c;
}

// Write a null-terminated string to UART.
void uart_puts(char *str)
{
  if (str == (char *)0)
  {
    return;
  }

  while (*str != '\0')
  {
    uart_putchar(*str);
    str++;
  }
}

// Write a signed integer in decimal format.
void uart_putint(int value)
{
  char buffer[12];
  itoa(value, buffer, 10);
  uart_puts(buffer);
}

// Write an unsigned integer in hexadecimal format.
void uart_puthex(unsigned int value, int prefix)
{
  char buffer[9];
  if (prefix)
  {
    uart_puts("0x");
  }
  itoa(value, buffer, 16);
  uart_puts(buffer);
}

// Write len bytes from buf to UART.
void uart_write(char *buf, unsigned int len)
{
  unsigned int i;

  if (buf == (char *)0)
  {
    return;
  }

  for (i = 0; i < len; i++)
  {
    uart_putchar(buf[i]);
  }
}

// ---- Receive Functions ----
void uart_isr_handler()
{
  volatile int *rx_reg = (volatile int *)UART_RX_ADDR;
  char byte;
  int next_head;

  byte = (char)(*rx_reg);

  next_head = (rx_head + 1) % UART_RX_BUFFER_SIZE;

  if (next_head == rx_tail)
  {
    rx_overflow_flag = 1;
    return;
  }

  rx_buffer[rx_head] = byte;
  rx_head = next_head;
}

// Return the number of bytes currently available in RX buffer.
int uart_available()
{
  return rx_count();
}

// Read one byte from RX buffer; return -1 when empty.
int uart_read()
{
  char byte;

  if (rx_head == rx_tail)
  {
    return -1;
  }

  byte = rx_buffer[rx_tail];
  rx_tail = (rx_tail + 1) % UART_RX_BUFFER_SIZE;

  return (int)(unsigned char)byte;
}

// Peek the next byte in RX buffer without removing it.
int uart_peek()
{
  if (rx_head == rx_tail)
  {
    return -1;
  }

  return (int)(unsigned char)rx_buffer[rx_tail];
}

// Read up to len bytes from RX buffer into buf.
int uart_read_bytes(char *buf, int len)
{
  int count = 0;
  int byte;

  if (buf == (char *)0 || len <= 0)
  {
    return 0;
  }

  while (count < len)
  {
    byte = uart_read();
    if (byte < 0)
    {
      break;
    }
    buf[count] = (char)byte;
    count++;
  }

  return count;
}

// Read bytes until terminator is seen, buffer is full, or RX is empty.
int uart_read_until(char *buf, int len, char terminator)
{
  int count = 0;
  int byte;

  if (buf == (char *)0 || len <= 0)
  {
    return 0;
  }

  while (count < len)
  {
    byte = uart_read();
    if (byte < 0)
    {
      break;
    }
    buf[count] = (char)byte;
    count++;
    if ((char)byte == terminator)
    {
      break;
    }
  }

  return count;
}

// Read bytes until newline or buffer end.
int uart_read_line(char *buf, int len)
{
  return uart_read_until(buf, len, '\n');
}

// Clear all buffered RX data and overflow state.
void uart_flush_rx()
{
  rx_head = 0;
  rx_tail = 0;
  rx_overflow_flag = 0;
}

// Return and clear the RX overflow flag.
int uart_rx_overflow()
{
  int flag = rx_overflow_flag;
  rx_overflow_flag = 0; // Clear on read
  return flag;
}
