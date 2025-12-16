#define COMMON_STDLIB
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

void print_jedec_id(int m, int n, int o)
{
    term_puts("JEDEC ID: ");
    term_puthex(m, 1);
    term_puts(" ");
    term_puthex(n, 1);
    term_puts(" ");
    term_puthex(o, 1);
    term_puts("\n");
}



int main() {
    init();

    int cmd = 0x9f; // Read JEDEC ID command

    spi_0_select();
    spi_0_transfer(cmd);
    int m0 = spi_0_transfer(0x00);
    int n0 = spi_0_transfer(0x00);
    int o0 =spi_0_transfer(0x00);
    spi_0_deselect();
    
    spi_1_select();
    spi_1_transfer(cmd);
    int m1 = spi_1_transfer(0x00);
    int n1 = spi_1_transfer(0x00);
    int o1 =spi_1_transfer(0x00);
    spi_1_deselect();

    term_puts("SPI Flash 0:\n");
    print_jedec_id(m0, n0, o0);
    term_puts("SPI Flash 1:\n");
    print_jedec_id(m1, n1, o1);

    return 1;
}

void interrupt()
{

}
