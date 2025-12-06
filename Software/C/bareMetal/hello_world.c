// First C program to run on the FPGC, testing the character graphics library

#include "libs/kernel/gfx.c"
#include "libs/kernel/ascii_data.c"

int main() {
    char *s = "Hello FPGC!";

    // Initialize the graphics system
    GFX_init();

    // Copy the ASCII pattern table to VRAM
    GFX_copy_pattern_table();

    // Set up the default palette (white text on black background)
    GFX_copy_palette_table();

    // Print the hello message
    GFX_puts(s);

    return 0;
}

void interrupt()
{
    // No need to handle interrupts
}
