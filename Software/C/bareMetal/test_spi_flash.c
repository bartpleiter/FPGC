#define COMMON_STDLIB
#define COMMON_STRING
#include "libs/common/common.h"

#define KERNEL_SPI_FLASH
#define KERNEL_TERM
#define KERNEL_GPU_DATA_ASCII
#include "libs/kernel/kernel.h"

void init()
{
    // Reset GPU VRAM
    gpu_clear_vram();

    // Load default pattern and palette tables
    unsigned int* pattern_table = (unsigned int*)&DATA_ASCII_DEFAULT;
    gpu_load_pattern_table(pattern_table + 3); // +3 to skip function prologue

    unsigned int* palette_table = (unsigned int*)&DATA_PALETTE_DEFAULT;
    gpu_load_palette_table(palette_table + 3); // +3 to skip function prologue

    // Initialize terminal
    term_init();
}

void write_test_data(int spi_id, char* data, int length)
{
    // First erase the first sector
    spi_flash_erase_sector(spi_id, 0x000000);
    spi_flash_write_page(spi_id, 0x000000, data, length);
}

void read_and_print_first_16_bytes(int spi_id)
{
    int buffer[16];
    spi_flash_read_data(spi_id, 0x000000, buffer, 16);

    term_puts("First 16 bytes from SPI Flash ");
    term_puthex(spi_id, 1);
    term_puts(":\n");
    for (int i = 0; i < 16; i++)
    {
        term_puthex(buffer[i], 1);
        term_puts(" ");
    }
    term_puts("\n");
}

void read_and_print_jedec_id(int spi_id)
{
    int manufacturer_id, memory_type, capacity;
    spi_flash_read_jedec_id(spi_id, &manufacturer_id, &memory_type, &capacity);

    term_puts("SPI Flash ");
    term_puthex(spi_id, 1);
    term_puts(" JEDEC ID: ");
    term_puthex(manufacturer_id, 1);
    term_puts(" ");
    term_puthex(memory_type, 1);
    term_puts(" ");
    term_puthex(capacity, 1);
    term_puts("\n");
}

void read_and_print_as_string(int spi_id, int address, int length)
{
    int buffer[256]; // Max length 256 for safety
    if (length > 256) length = 256;

    spi_flash_read_data(spi_id, address, buffer, length);

    term_puts("Data from SPI Flash ");
    term_puthex(spi_id, 1);
    term_puts(" at address ");
    term_puthex(address, 6);
    term_puts(": ");

    for (int i = 0; i < length; i++)
    {
        char c = (char)(buffer[i] & 0xFF);
        term_putchar(c);
    }
    term_puts("\n");
}

int main() {
    init();

    read_and_print_jedec_id(SPI_FLASH_0);
    read_and_print_jedec_id(SPI_FLASH_1);
    
    // Writes are commented out for safety
    char* test_data_0 = "Hello World!";
    //write_test_data(SPI_FLASH_0, test_data_0, 12);

    char* test_data_1 = "Yo Waddup!";
    //write_test_data(SPI_FLASH_1, test_data_1, 9);

    read_and_print_first_16_bytes(SPI_FLASH_0);
    read_and_print_first_16_bytes(SPI_FLASH_1);

    read_and_print_as_string(SPI_FLASH_0, 0x000000, 12);
    read_and_print_as_string(SPI_FLASH_1, 0x000000, 9);

    return 1;
}

void interrupt()
{

}
