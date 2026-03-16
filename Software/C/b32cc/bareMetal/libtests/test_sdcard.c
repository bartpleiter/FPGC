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

    term_puts("SD Card SPI test\n");

    // 1. Send dummy clocks with CS high (card needs this on power-up)
    spi_deselect(SPI_ID_SD_CARD);
    for(int i = 0; i < 10; i++) {
        spi_transfer(SPI_ID_SD_CARD, 0xFF);
    }

    // 2. Select the card
    spi_select(SPI_ID_SD_CARD);

    // 3. Send CMD0 (GO_IDLE_STATE) to reset the card
    // Format: 0x40 | cmd, arg[4 bytes], crc, 0xFF
    spi_transfer(SPI_ID_SD_CARD, 0x40);  // CMD0
    spi_transfer(SPI_ID_SD_CARD, 0x00);  // arg
    spi_transfer(SPI_ID_SD_CARD, 0x00);
    spi_transfer(SPI_ID_SD_CARD, 0x00);
    spi_transfer(SPI_ID_SD_CARD, 0x00);
    spi_transfer(SPI_ID_SD_CARD, 0x95);  // CRC for CMD0 (only CMD0 and CMD8 need valid CRC)

    // 4. Wait for response (R1 format)
    // The card will send 0xFF until it's ready, then send R1
    for(int i = 0; i < 10; i++) {
        response = spi_transfer(SPI_ID_SD_CARD, 0xFF);
        if(response != 0xFF) break;
    }

    // 5. Send a final clock cycle
    spi_transfer(SPI_ID_SD_CARD, 0xFF);
    
    spi_deselect(SPI_ID_SD_CARD);

    // 6. Check response
    // Should be 0x01 (idle state) if successful
    if(response == 0x01) {
        term_puts("SD Card initialized successfully (R1 response: 0x01)\n");
    } else {
        term_puts("SD Card initialization failed (R1 response: ");
        term_puthex(response, 1);
        term_puts(")\n");
    }

    return 1;
}

void interrupt()
{

}
