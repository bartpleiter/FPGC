#define COMMON_STDLIB
#define COMMON_STRING
#include "libs/common/common.h"

#define KERNEL_UART
#define KERNEL_TERM
#include "libs/kernel/kernel.h"

int main()
{
    char* msg = "Hello\nWorld!\n";
    for (size_t i = 0; i < strlen(msg); i++)
    {
        uart_send_char(msg[i]);
    }
}

void interrupt()
{
}
