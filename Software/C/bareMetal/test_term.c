// Test program for Terminal Library
#define KERNEL_TERM
#define KERNEL_GPU_DATA_ASCII
#include "libs/kernel/kernel.h"

#define LINE_PER_FRAMES 30

int frame_drawn = 0;
int second_passed = 0;

int main() {
    // Reset GPU VRAM
    gpu_clear_vram();

    // Load default pattern and palette tables
    unsigned int* pattern_table = (unsigned int*)&DATA_ASCII_DEFAULT;
    gpu_load_pattern_table(pattern_table + 3); // +3 to skip function prologue

    unsigned int* palette_table = (unsigned int*)&DATA_PALETTE_DEFAULT;
    gpu_load_palette_table(palette_table + 3); // +3 to skip function prologue

    // Initialize terminal
    term_init();

    // Test 1: Basic text output
    term_puts("Hello, World!\n");
    term_puts("Terminal Library Test\n");
    term_puts("\n");

    // Test 2: Special characters
    term_puts("Testing special chars:\n");
    term_puts("Tab\there\tand\there\n");
    term_puts("Carriage return test\r[CR]\n");
    term_puts("\n");

    // Test 3: Palette colors
    term_set_palette(1);
    term_puts("Color 1 text\n");
    term_set_palette(2);
    term_puts("Color 2 text\n");
    term_set_palette(3);
    term_puts("Color 3 text\n");
    term_set_palette(0);
    term_puts("\n");

    // Test 4: Cursor positioning
    term_puts("Cursor positioning test:\n");
    unsigned int x, y;
    term_get_cursor(&x, &y);
    term_set_cursor(10, 10);
    term_puts("At (10,10)");
    term_set_cursor(20, 12);
    term_puts("At (20,12)");
    term_set_cursor(0, 13);
    term_puts("\n");

    // Test 5: term_write with buffer
    char buf[] = "Buffer test: 12345";
    term_write(buf, 18);
    term_putchar('\n');
    term_puts("\n");


    int lines_drawn = 0;

    while (1)
    {
        if (second_passed)
        {
            second_passed = 0;
            term_puts("This is line number: ");
            term_putchar('0' + lines_drawn);
            term_putchar('\n');
            lines_drawn++;
            lines_drawn %= 10; // Keep it to single digit
        }
    }    
    return 1;
}

void interrupt() {
    frame_drawn++;
    if (frame_drawn >= LINE_PER_FRAMES) {
        second_passed = 1;
        frame_drawn = 0;
    }
}
