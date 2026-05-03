#include "bdos.h"

/* libterm wiring: render callback pushes a single cell to the GPU
   window plane; uart callback mirrors printable bytes to the UART. */
static void bdos_term_render_cb(int x, int y,
                                unsigned char tile, unsigned char palette)
{
  gpu_write_window_tile((unsigned int)x, (unsigned int)y, tile, palette);
}

static void bdos_term_uart_cb(char c) { uart_putchar(c); }

static void bdos_init_gpu(void)
{
  gpu_clear_vram();

  gpu_load_pattern_table(gpu_default_patterns);
  gpu_load_palette_table(gpu_default_palette);

  gpu_reset_pixel_palette();
}

static void bdos_init_usb_keyboard(void)
{
  term_puts("Initializing CH376 (ID ");
  term_putint(bdos_usb_keyboard_spi_id);
  term_puts(") for input\n");

  if (!ch376_host_init(bdos_usb_keyboard_spi_id))
  {
    bdos_panic("Failed to initialize CH376 USB host");
  }

  memset(&bdos_usb_keyboard_device, 0, sizeof(usb_device_info_t));

  timer_set_callback(TIMER_1, bdos_poll_usb_keyboard);
  timer_start_periodic(TIMER_1, 10);
}

static void bdos_init_ethernet(void)
{
  int bdos_eth_mac[6];
  int rev;
  int i;
  int spi_flash_id_buffer[8];

  term_puts("Initializing ENC28J60 Ethernet\n");

  bdos_eth_mac[0] = 0x02;
  bdos_eth_mac[1] = 0xB4;
  bdos_eth_mac[2] = 0xB4;
  bdos_eth_mac[3] = 0x00;
  bdos_eth_mac[4] = 0x00;

  spi_flash_read_unique_id(SPI_FLASH_0, spi_flash_id_buffer);
  switch (spi_flash_id_buffer[6])
  {
  case 0x58:
    bdos_eth_mac[5] = 0x01;
    break;
  case 0x4b:
    bdos_eth_mac[5] = 0x02;
    break;
  case 0x40:
    bdos_eth_mac[5] = 0x03;
    break;
  case 0x2a:
    bdos_eth_mac[5] = 0x04;
    break;
  case 0x46:
    bdos_eth_mac[5] = 0x05;
    break;
  default:
    bdos_panic("Unknown SPI Flash unique ID, cannot determine MAC address");
  }

  rev = enc28j60_init(bdos_eth_mac);
  if (rev == 0)
  {
    bdos_panic("ENC28J60 init failed (rev=0)\n");
  }

  i = 0;
  while (i < 6)
  {
    fnp_our_mac[i] = bdos_eth_mac[i];
    i = i + 1;
  }

  bdos_fnp_init();
}

void bdos_init(void)
{
  set_user_led(1);

  spi_deselect(SPI_FLASH_0);

  bdos_init_gpu();


  gpu_set_window_palette(0);

  term_init(TERM_WIDTH, TERM_HEIGHT,
             bdos_term_render_cb, bdos_term_uart_cb);
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

  bdos_slot_init();
  term_puts("Program slot system initialized\n");

  bdos_heap_init();
  term_puts("Heap allocator initialized\n");

  bdos_proc_init();
  term_puts("VFS + process model initialized\n");

  term_puts("Mounting filesystems\n");
  term_puts("  SPI flash\n");
  bdos_fs_boot_init();
  
  term_puts("  SD card\n");
  bdos_fs_sd_init();

  set_user_led(0);
}
