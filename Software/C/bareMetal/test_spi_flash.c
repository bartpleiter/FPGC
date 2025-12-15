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

// TODO create functions like this in stdlib
void write_int(int value)
{
    char buffer[14];
    buffer[0] = '0';
    buffer[1] = 'x';
    itoa(value, &buffer[2], 16);
    term_puts(buffer);
    term_putchar('\n');
}

int main() {
    init();

    int cmd = 0x9f; // Read JEDEC ID command
    spi_1_select();


    spi_1_transfer(cmd);
    int m = spi_1_transfer(0x00);
    write_int(m);
    m = spi_1_transfer(0x00);
    write_int(m);
    m = spi_1_transfer(0x00);
    write_int(m);


    spi_1_deselect();
    return 1;
}

void interrupt()
{

}
