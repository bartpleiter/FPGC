// First C program to run on the FPGC, testing the character graphics library
#include "libs/kernel/gfx.c"
#include "libs/kernel/gpu_data_ascii.c"

int main() {

    char* msg = "Yo Waddup!\nThis is a test of the\ngraphics library.\n";

    GFX_init();

    // Copy the ASCII pattern table to VRAM
    unsigned int* pattern_table = (unsigned int*)&DATA_ASCII_DEFAULT;
    GFX_copy_pattern_table(pattern_table + 3); // +3 to skip function prologue

    // Copy the palette table to VRAM
    unsigned int* palette_table = (unsigned int*)&DATA_PALETTE_DEFAULT;
    GFX_copy_palette_table(palette_table + 3); // +3 to skip function prologue

    GFX_puts(msg);

    return 0x39;
}

void interrupt()
{
    // No need to handle interrupts
}
