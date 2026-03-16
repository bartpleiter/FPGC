#define COMMON_STDLIB
#define COMMON_STRING
#include "libs/common/common.h"

#define KERNEL_SPI
#define KERNEL_TERM
#define KERNEL_GPU_DATA_ASCII
#include "libs/kernel/kernel.h"

// ENC28J60 opcodes
#define ENC28J60_WRITE_CTRL_REG 0x40
#define ENC28J60_READ_CTRL_REG  0x00

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

    int reg_addr = 0x00;  // ERDPTL register
    int data = 0x34;
    int read_back;
    
    // Write to register
    spi_select(SPI_ID_ETH);
    spi_transfer(SPI_ID_ETH, ENC28J60_WRITE_CTRL_REG | reg_addr);
    spi_transfer(SPI_ID_ETH, data);
    spi_deselect(SPI_ID_ETH);
    
    // Read back from register
    spi_select(SPI_ID_ETH);
    spi_transfer(SPI_ID_ETH, ENC28J60_READ_CTRL_REG | reg_addr);
    read_back = spi_transfer(SPI_ID_ETH, 0x00);  // Send dummy byte to clock in data
    spi_deselect(SPI_ID_ETH);
    
    // Verify
    term_puts("ENC28J60 Register Read/Write Test\n");
    term_puts("Wrote: ");
    term_puthex(data, 1);
    term_puts("\nRead:  ");
    term_puthex(read_back, 1);
    term_puts("\n");
        

    return 1;
}

void interrupt()
{

}
