/*
 * FPGC Flash Writer
 * Flashes binary from flash_binary.c to SPI flash
 */

#include "bareMetal/flash_writer/flash_binary.c"

#define SPI_FLASH_TO_USE SPI_FLASH_0

#define COMMON_STDLIB
#define COMMON_STRING
#include "libs/common/common.h"

#define KERNEL_SPI_FLASH
#define KERNEL_UART
#include "libs/kernel/kernel.h"

void clear_flash()
{
    uart_puts("Erasing flash...\n");
    
    int i;
    int binary_word_size = FLASH_PROGRAM_SIZE_WORDS; // Number of 32-bit words in flash_binary
    // Loop over spi_flash_erase_block_64k
    for (i = 0; i < binary_word_size * 4; i += 16384) // 64KB = 16384 words
    {
        uart_puts(" Erasing 64KB block at address ");
        uart_puthex(i * 4, 1); // Address in bytes
        uart_puts("...\n");
        spi_flash_erase_block_64k(SPI_FLASH_TO_USE, i * 4); // Address in bytes
    }

    uart_puts("Flash erased.\n\n");
}

void write_flash()
{
    uart_puts("Writing flash...\n");

    int i;
    int binary_word_size = FLASH_PROGRAM_SIZE_WORDS; // Number of 32-bit words in flash_binary
    unsigned int* binary_data = (unsigned int*) (&flash_binary);
    binary_data += 3; // Skip function prologue
    
    // Write in pages of 64 words (256 bytes)
    for (i = 0; i < binary_word_size; i += 64)
    {
        int words_to_write = (binary_word_size - i) < 64 ? (binary_word_size - i) : 64;
        
        uart_puts(" Writing ");
        uart_putint(words_to_write);
        uart_puts(" words at address ");
        uart_puthex(i * 4, 1); // Address in bytes
        uart_puts("...\n");
        
        spi_flash_write_words(SPI_FLASH_TO_USE, i * 4, &binary_data[i], words_to_write);
    }

    uart_puts("Flash write complete.\n\n");
}

void debug_check_uart()
{
    // Read the first 8 words back from flash and print them over UART
    uart_puts("Verifying first 8 words of flash contents...\n");
    unsigned int read_buffer[8];
    spi_flash_read_words(SPI_FLASH_TO_USE, 0, read_buffer, 8);
    int j;
    for (j = 0; j < 8; j++)
    {
        uart_puts(" Word ");;
        uart_putint(j);
        uart_puts(": 0x");
        uart_puthex(read_buffer[j], 1);
        uart_puts("\n");
    }
}

int main() {
    uart_puts("FPGC Flash Writer\n\n");
    clear_flash();
    write_flash();

    debug_check_uart();

    return 1;
}

void interrupt()
{

}
