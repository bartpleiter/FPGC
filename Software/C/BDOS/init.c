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

void bdos_init_usb_keyboard()
{
  term_puts("Initializing CH376 (ID ");
  term_putint(bdos_usb_keyboard_spi_id);
  term_puts(") for keyboard\n");

  if (!ch376_host_init(bdos_usb_keyboard_spi_id))
  {
    bdos_panic("Failed to initialize CH376 USB host");
  }
  term_puts("CH376 USB host initialized\n");

  // Initialize bdos_usb_keyboard_device struct with default values
  memset(&bdos_usb_keyboard_device, 0, sizeof(usb_device_info_t));

  // Setup polling timer for keyboard
  timer_set_callback(TIMER_1, bdos_poll_usb_keyboard);
  timer_start_periodic(TIMER_1, 10); // Poll every 10ms
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
  // we can print to the terminal for progress indication
  term_puts("GPU initialized\n");

  // Initialize timer subsystem
  timer_init();
  term_puts("Timers initialized\n");

  // Initialize UART subsystem
  uart_init();
  term_puts("UART initialized\n");

  // Initialize USB host subsystem for keyboard polling
  bdos_init_usb_keyboard();
}
