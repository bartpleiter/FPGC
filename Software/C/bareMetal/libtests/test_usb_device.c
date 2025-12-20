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

int main()
{

    init();

    int response;

    term_puts("USB (CH376) test\n");

    int cmd_get_ic_ver = 0x01;
    int cmd_set_usb_mode = 0x15;
    int cmd_get_status = 0x22;
    int cmd_disk_connect = 0x30;
    
    int usb_mode_host = 0x06;  // USB host mode

    // Test IC 0
    term_puts("\n=== Testing CH376 IC 0 ===\n");
    
    spi_select(SPI_ID_USB_0);
    spi_transfer(SPI_ID_USB_0, cmd_get_ic_ver);
    response = spi_transfer(SPI_ID_USB_0, 0xFF);
    spi_deselect(SPI_ID_USB_0);

    term_puts("CH376 IC 0 Version: ");
    term_puthex(response, 1);
    term_puts("\n");

    // Set USB mode to host
    spi_select(SPI_ID_USB_0);
    spi_transfer(SPI_ID_USB_0, cmd_set_usb_mode);
    spi_transfer(SPI_ID_USB_0, usb_mode_host);
    spi_deselect(SPI_ID_USB_0);

    int i;
    
    // Small delay for mode change
    for(i = 0; i < 10000; i++);
    
    // Get status after mode change
    spi_select(SPI_ID_USB_0);
    spi_transfer(SPI_ID_USB_0, cmd_get_status);
    response = spi_transfer(SPI_ID_USB_0, 0xFF);
    spi_deselect(SPI_ID_USB_0);
    
    term_puts("IC 0 USB Mode Status: ");
    term_puthex(response, 1);
    term_puts("\n");

    // Check for USB device connection
    spi_select(SPI_ID_USB_0);
    spi_transfer(SPI_ID_USB_0, cmd_disk_connect);
    spi_deselect(SPI_ID_USB_0);
    
    // Delay for connection check
    for(i = 0; i < 50000; i++);
    
    spi_select(SPI_ID_USB_0);
    spi_transfer(SPI_ID_USB_0, cmd_get_status);
    response = spi_transfer(SPI_ID_USB_0, 0xFF);
    spi_deselect(SPI_ID_USB_0);
    
    term_puts("IC 0 Connection Status: ");
    term_puthex(response, 1);
    if(response == 0x14) {
        term_puts(" - USB device connected!\n");
    } else if(response == 0x15) {
        term_puts(" - No USB device detected\n");
    } else {
        term_puts(" - Unknown status\n");
    }

    // Test IC 1
    term_puts("\n=== Testing CH376 IC 1 ===\n");
    
    spi_select(SPI_ID_USB_1);
    spi_transfer(SPI_ID_USB_1, cmd_get_ic_ver);
    response = spi_transfer(SPI_ID_USB_1, 0xFF);
    spi_deselect(SPI_ID_USB_1);

    term_puts("CH376 IC 1 Version: ");
    term_puthex(response, 1);
    term_puts("\n");

    // Set USB mode to host
    spi_select(SPI_ID_USB_1);
    spi_transfer(SPI_ID_USB_1, cmd_set_usb_mode);
    spi_transfer(SPI_ID_USB_1, usb_mode_host);
    spi_deselect(SPI_ID_USB_1);
    
    // Small delay for mode change
    for(i = 0; i < 10000; i++);
    
    // Get status after mode change
    spi_select(SPI_ID_USB_1);
    spi_transfer(SPI_ID_USB_1, cmd_get_status);
    response = spi_transfer(SPI_ID_USB_1, 0xFF);
    spi_deselect(SPI_ID_USB_1);
    
    term_puts("IC 1 USB Mode Status: ");
    term_puthex(response, 1);
    term_puts("\n");

    // Check for USB device connection
    spi_select(SPI_ID_USB_1);
    spi_transfer(SPI_ID_USB_1, cmd_disk_connect);
    spi_deselect(SPI_ID_USB_1);
    
    // Delay for connection check
    for(i = 0; i < 50000; i++);
    
    spi_select(SPI_ID_USB_1);
    spi_transfer(SPI_ID_USB_1, cmd_get_status);
    response = spi_transfer(SPI_ID_USB_1, 0xFF);
    spi_deselect(SPI_ID_USB_1);
    
    term_puts("IC 1 Connection Status: ");
    term_puthex(response, 1);
    if(response == 0x14) {
        term_puts(" - USB device connected!\n");
    } else if(response == 0x15) {
        term_puts(" - No USB device detected\n");
    } else {
        term_puts(" - Unknown status\n");
    }

    return 1;
}

void interrupt()
{

}
