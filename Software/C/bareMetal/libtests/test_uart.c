#define COMMON_STDLIB
#include "libs/common/common.h"

#define KERNEL_MEM_DEBUG
#include "libs/kernel/kernel.h"

int main()
{
    char* msg = "Hello\nWorld!\n";
    uart_puts(msg);
    uart_puthex(0xDEADBEEF, 1);
    uart_putchar('\n');
    uart_putint(12345);
    uart_putchar('\n');

    debug_mem_dump(msg, 32);

    unsigned int micros = get_micros();
    uart_puts("\nMicros: ");
    uart_putint(micros);
    uart_putchar('\n');

    return 0;
}

void interrupt()
{
}
