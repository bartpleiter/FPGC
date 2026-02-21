//
// BDOS initialization module.
//

#include "BDOS/bdos.h"

void bdos_init_gpu()
{
  unsigned int* pattern_table;
  unsigned int* palette_table;

  gpu_clear_vram();

  // Load default ASCII table and palette
  pattern_table = (unsigned int*)&DATA_ASCII_DEFAULT;
  gpu_load_pattern_table(pattern_table + 3); // +3 to skip function prologue

  palette_table = (unsigned int*)&DATA_PALETTE_DEFAULT;
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

  memset(&bdos_usb_keyboard_device, 0, sizeof(usb_device_info_t));

  // Setup polling timer
  timer_set_callback(TIMER_1, bdos_poll_usb_keyboard);
  timer_start_periodic(TIMER_1, 10); // Poll every 10ms
}

void bdos_init_ethernet()
{
  int bdos_eth_mac[6];
  int rev;
  int i;
  
  term_puts("Initializing ENC28J60 Ethernet\n");

  // MAC address: 02:B4:B4:00:00:01
  // TODO: Make configurable from flash for FPGC cluster
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

  // Store our MAC in FNP state for frame building
  i = 0;
  while (i < 6)
  {
    fnp_our_mac[i] = bdos_eth_mac[i];
    i = i + 1;
  }

  // Initialize FNP protocol state
  bdos_fnp_init();
}

void bdos_init()
{
  set_user_led(1); // Set user LED during initialization to indicate progress

  bdos_init_gpu();

  term_init();
  term_set_palette(PALETTE_WHITE_ON_BLACK);
  term_puts("GPU initialized\n");

  timer_init();
  term_puts("Timers initialized\n");

  uart_init();
  term_puts("UART initialized\n");

  bdos_init_ethernet();
  term_puts("ENC28J60 Ethernet initialized\n");

  bdos_init_usb_keyboard();
  term_puts("CH376 USB host initialized\n");

  set_user_led(0); // Turn off user LED after initialization
}
