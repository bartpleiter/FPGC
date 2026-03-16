/*
 * FPGC Flash Writer
 * Flashes binary from flash_binary.c to SPI flash
 */

#include "flash_binary.c"

#include <spi.h>
#include <spi_flash.h>
#include <uart.h>
#include <stdio.h>

#define SPI_FLASH_TO_USE SPI_FLASH_0

void clear_flash()
{
    uart_puts("Erasing flash...\n");
    
    int i;
    int binary_word_size = FLASH_PROGRAM_SIZE_WORDS;
    for (i = 0; i < binary_word_size; i += 16384) // 64KB = 16384 words
    {
        uart_puts(" Erasing 64KB block at address ");
        uart_puthex(i * 4, 1);
        uart_puts("...\n");
        spi_flash_erase_block_64k(SPI_FLASH_TO_USE, i * 4);
    }

    uart_puts("Flash erased.\n\n");
}

void write_flash()
{
    uart_puts("Writing flash...\n");

    int i;
    int binary_word_size = FLASH_PROGRAM_SIZE_WORDS;
    const unsigned int* binary_data = flash_binary;
    
    // Write in pages of 64 words (256 bytes)
    for (i = 0; i < binary_word_size; i += 64)
    {
        int words_to_write = (binary_word_size - i) < 64 ? (binary_word_size - i) : 64;
        
        uart_puts(" Writing ");
        uart_putint(words_to_write * 4);
        uart_puts(" bytes at address ");
        uart_puthex(i * 4, 1);
        uart_puts("...\n");
        
        // Cast away const — spi_flash_write_words takes unsigned int*
        spi_flash_write_words(SPI_FLASH_TO_USE, i * 4, (unsigned int*)&binary_data[i], words_to_write);
    }

    uart_puts("Flash write complete.\n\n");
}

void debug_check_uart()
{
    uart_puts("Verifying first 32 bytes of flash contents...\n");
    unsigned int read_buffer[8];
    spi_flash_read_words(SPI_FLASH_TO_USE, 0, read_buffer, 8);
    int j;
    for (j = 0; j < 8; j++)
    {
        uart_puts(" Word ");
        uart_putint(j);
        uart_puts(": ");
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
