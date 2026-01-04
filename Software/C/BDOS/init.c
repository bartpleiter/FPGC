/*
 * BDOS initialization module.
 */

#include "BDOS/bdos.h"

void bdos_init_gpu()
{
  // Reset all GPU VRAM
  gpu_clear_vram();

  // Load default ASCII pattern and palette tables
  unsigned int* pattern_table = (unsigned int*)&DATA_ASCII_DEFAULT;
  gpu_load_pattern_table(pattern_table + 3); // +3 to skip function prologue

  unsigned int* palette_table = (unsigned int*)&DATA_PALETTE_DEFAULT;
  gpu_load_palette_table(palette_table + 3); // +3 to skip function prologue
}

// BDOS initialization function to be called from main
void bdos_init()
{
  // Initialize GPU
  bdos_init_gpu();

  // Initialize terminal
  term_init();
  term_set_palette(PALETTE_WHITE_ON_BLACK);

  // Since we now initialized the GPU and terminal,
  // we can print now to the terminal for progress indication
  term_puts("GPU and terminal initialized\n");

  // Initialize timer subsystem
  timer_init();
  term_puts("Timers initialized\n");

  // Initialize UART subsystem
  uart_init();
  term_puts("UART initialized\n");
}
