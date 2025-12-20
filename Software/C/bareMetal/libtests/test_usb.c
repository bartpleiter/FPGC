#define COMMON_STDLIB
#define COMMON_STRING
#include "libs/common/common.h"

#define KERNEL_SPI
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


int main() {
    init();

    int response;

    term_puts("USB (CH376) test\n");

    int cmd_get_ic_ver = 0x01;

    spi_select(SPI_ID_USB_0);
    spi_transfer(SPI_ID_USB_0, cmd_get_ic_ver);
    response = spi_transfer(SPI_ID_USB_0, 0xFF);
    spi_deselect(SPI_ID_USB_0);

    term_puts("CH376 IC 0 Version: ");
    term_puthex(response, 1);
    term_puts("\n");

    spi_select(SPI_ID_USB_1);
    spi_transfer(SPI_ID_USB_1, cmd_get_ic_ver);
    response = spi_transfer(SPI_ID_USB_1, 0xFF);
    spi_deselect(SPI_ID_USB_1);

    term_puts("CH376 IC 1 Version: ");
    term_puthex(response, 1);
    term_puts("\n");

    return 1;
}

void interrupt()
{

}
