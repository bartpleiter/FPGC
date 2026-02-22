#define COMMON_STDLIB
#define COMMON_STRING
#include "libs/common/common.h"

#define KERNEL_UART
#define KERNEL_SPI_FLASH
#include "libs/kernel/kernel.h"




int main()
{
  int id_buffer_1[8];
  int id_buffer_2[8];

  int id_buffer_1[8];
  spi_flash_read_unique_id(SPI_FLASH_0, id_buffer_1);
  spi_flash_read_unique_id(SPI_FLASH_1, id_buffer_2);

  uart_puts("SPI Flash 0 Unique ID: ");
  for (int i = 0; i < 8; i++)
  {
    uart_puthex(id_buffer_1[i], 1);
    uart_puts(" ");
  }
  uart_puts("\n");

  uart_puts("SPI Flash 1 Unique ID: ");
  for (int i = 0; i < 8; i++)
  {
    uart_puthex(id_buffer_2[i], 1);
    uart_puts(" ");
  }
  uart_puts("\n");

  return 1;
}

void interrupt()
{

}
