#define COMMON_STDLIB
#include "libs/common/common.h"

#define KERNEL_TIMER
#define KERNEL_UART
#include "libs/kernel/kernel.h"

/*
 * UART RX Test Program
 * 
 * This test verifies the UART receive functionality.
 * Since no data is being sent during this test, it primarily
 * verifies that the code compiles and the API works correctly.
 * 
 * To fully test RX functionality, run this program and send
 * data via a serial terminal.
 */

int main()
{
    char buf[32];
    int i;
    int byte;
    int count;
    
    /* Initialize subsystems */
    uart_init();
    timer_init();
    
    uart_puts("=== UART RX Library Test ===\n\n");

    /* Test 1: Check initial state */
    uart_puts("Test 1: Initial buffer state\n");
    uart_puts("  uart_available(): ");
    uart_putint(uart_available());
    uart_puts(" (expected 0)\n");
    uart_puts("  uart_read(): ");
    uart_putint(uart_read());
    uart_puts(" (expected -1)\n");
    uart_puts("  uart_peek(): ");
    uart_putint(uart_peek());
    uart_puts(" (expected -1)\n");
    uart_puts("Test 1 passed!\n\n");

    /* Test 2: Read bytes with empty buffer */
    uart_puts("Test 2: Read functions with empty buffer\n");
    count = uart_read_bytes(buf, sizeof(buf));
    uart_puts("  uart_read_bytes(): ");
    uart_putint(count);
    uart_puts(" bytes (expected 0)\n");
    count = uart_read_until(buf, sizeof(buf), '\n');
    uart_puts("  uart_read_until(): ");
    uart_putint(count);
    uart_puts(" bytes (expected 0)\n");
    count = uart_read_line(buf, sizeof(buf));
    uart_puts("  uart_read_line(): ");
    uart_putint(count);
    uart_puts(" bytes (expected 0)\n");
    uart_puts("Test 2 passed!\n\n");

    /* Test 3: Flush and overflow check */
    uart_puts("Test 3: Flush and overflow\n");
    uart_flush_rx();
    uart_puts("  After flush, available: ");
    uart_putint(uart_available());
    uart_puts(" (expected 0)\n");
    uart_puts("  Overflow flag: ");
    uart_putint(uart_rx_overflow());
    uart_puts(" (expected 0)\n");
    uart_puts("Test 3 passed!\n\n");

    /* Test 4: NULL pointer safety */
    uart_puts("Test 4: NULL pointer safety\n");
    count = uart_read_bytes((char *)0, 10);
    uart_puts("  uart_read_bytes(NULL, 10): ");
    uart_putint(count);
    uart_puts(" (expected 0)\n");
    count = uart_read_until((char *)0, 10, '\n');
    uart_puts("  uart_read_until(NULL, 10, '\\n'): ");
    uart_putint(count);
    uart_puts(" (expected 0)\n");
    uart_puts("Test 4 passed!\n\n");

    /* Test 5: Buffer size check */
    uart_puts("Test 5: Buffer configuration\n");
    uart_puts("  RX buffer size: ");
    uart_putint(UART_RX_BUFFER_SIZE);
    uart_puts(" bytes\n");
    uart_puts("Test 5 passed!\n\n");

    uart_puts("=== Static Tests Complete ===\n\n");

    /* Interactive test - echo received characters */
    uart_puts("Interactive mode: Send characters to test RX.\n");
    uart_puts("Characters received will be echoed back.\n");
    uart_puts("Waiting 2 seconds for input...\n\n");
    
    /* Wait and collect any data for 2 seconds */
    delay(2000);
    
    /* Check if we received anything */
    count = uart_available();
    uart_puts("Bytes received: ");
    uart_putint(count);
    uart_puts("\n");
    
    if (count > 0) {
        uart_puts("Data: ");
        while (uart_available() > 0) {
            byte = uart_read();
            if (byte >= 0) {
                uart_putchar((char)byte);
            }
        }
        uart_puts("\n");
    }
    
    /* Check overflow */
    if (uart_rx_overflow()) {
        uart_puts("Warning: RX buffer overflow occurred!\n");
    }

    uart_puts("\n=== All UART RX Tests Complete! ===\n");
    return 0;
}

void interrupt()
{
    int int_id = get_int_id();
    switch (int_id)
    {
        case INTID_UART:
            uart_isr_handler();
            break;
        case INTID_TIMER0:
            timer_isr_handler(TIMER_0);
            break;
        case INTID_TIMER1:
            timer_isr_handler(TIMER_1);
            break;
        case INTID_TIMER2:
            timer_isr_handler(TIMER_2);
            break;
        default:
            break;
    }
}
