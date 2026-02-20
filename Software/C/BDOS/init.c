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
  term_puts(") for input\n");

  if (!ch376_host_init(bdos_usb_keyboard_spi_id))
  {
    bdos_panic("Failed to initialize CH376 USB host");
  }

  // Initialize bdos_usb_keyboard_device struct with default values
  memset(&bdos_usb_keyboard_device, 0, sizeof(usb_device_info_t));

  // Setup polling timer for keyboard
  timer_set_callback(TIMER_1, bdos_poll_usb_keyboard);
  timer_start_periodic(TIMER_1, 10); // Poll every 10ms

  term_puts("CH376 USB host initialized\n");
}

void bdos_init_ethernet()
{
  int bdos_eth_mac[6];
  int rev;
  
  term_puts("Initializing ENC28J60 Ethernet\n");

  // Set MAC address (02:B4:B4:00:00:01)
  // TODO: Make this configurable from flash when working on FPGC cluster
  bdos_eth_mac[0] = 0x02;
  bdos_eth_mac[1] = 0xB4;
  bdos_eth_mac[2] = 0xB4;
  bdos_eth_mac[3] = 0x00;
  bdos_eth_mac[4] = 0x00;
  bdos_eth_mac[5] = 0x01;

  rev = enc28j60_init(bdos_eth_mac);
  if (rev == 0)
  {
    bdos_panic("ENC28J60 init failed (rev=0)\n");
  }

  // Initialize RX ring buffer
  {
    int i;
    i = 0;
    while (i < BDOS_ETH_RX_SLOTS)
    {
      bdos_eth_rx_len[i] = 0;
      i = i + 1;
    }
  }
  bdos_eth_rx_head = 0;
  bdos_eth_rx_tail = 0;

  // Setup periodic timer to poll the ENC28J60
  timer_set_callback(TIMER_0, bdos_poll_ethernet);
  timer_start_periodic(TIMER_0, 10);

  term_puts("ENC28J60 Ethernet initialized\n");
}

// BDOS initialization function to be called from main
void bdos_init()
{
  // Indicate core initialization in progress by turning on user LED
  set_user_led(1);

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

  // Initialize Ethernet controller
  bdos_init_ethernet();

  // Initialize USB host subsystem for keyboard polling
  bdos_init_usb_keyboard();

  // Core initialization complete, turn off user LED
  set_user_led(0);
}
