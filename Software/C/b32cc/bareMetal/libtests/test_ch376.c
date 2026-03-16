#define COMMON_STDLIB
#define COMMON_STRING
#include "libs/common/common.h"

#define KERNEL_TIMER
#define KERNEL_CH376
#define KERNEL_TERM
#define KERNEL_GPU_DATA_ASCII
#include "libs/kernel/kernel.h"

void init()
{
  // Reset GPU VRAM
  gpu_clear_vram();

  // Load default pattern and palette tables
  unsigned int *pattern_table = (unsigned int *)&DATA_ASCII_DEFAULT;
  gpu_load_pattern_table(pattern_table + 3); // +3 to skip function prologue

  unsigned int *palette_table = (unsigned int *)&DATA_PALETTE_DEFAULT;
  gpu_load_palette_table(palette_table + 3); // +3 to skip function prologue

  // Initialize terminal
  term_init();
}

/* Print device descriptor info */
void print_device_info(usb_device_info_t *info)
{
  term_puts("USB Device Info:\n");
  term_set_palette(PALETTE_CYAN_ON_BLACK);
  term_puts(" VID: ");
  term_puthex(info->device_desc.idVendor, 1);
  term_puts(" PID: ");
  term_puthex(info->device_desc.idProduct, 1);
  term_putchar('\n');

  term_puts(" Cls: ");
  term_puthex(info->device_desc.bDeviceClass, 1);
  term_puts(" SubCls: ");
  term_puthex(info->device_desc.bDeviceSubClass, 1);
  term_putchar('\n');

  term_puts(" Iface Cls: ");
  term_puthex(info->interface_class, 1);
  term_puts(" SubCls: ");
  term_puthex(info->interface_subclass, 1);
  term_puts(" Prot: ");
  term_puthex(info->interface_protocol, 1);
  term_putchar('\n');

  if (info->low_speed)
  {
    term_puts(" Spd: Low\n");
  }
  else
  {
    term_puts(" Spd: Full\n");
  }

  if (ch376_is_keyboard(info))
  {
    term_puts(" Type: HID Keyboard\n");
    term_puts(" Interrupt EP: ");
    term_puthex(info->interrupt_endpoint, 1);
    term_putchar('\n');
  }
  else if (ch376_is_mouse(info))
  {
    term_puts(" Type: HID Mouse\n");
  }
  else
  {
    term_puts(" Type: Unknown\n");
  }
  term_set_palette(PALETTE_WHITE_ON_BLACK);
}

void print_kb_report(hid_keyboard_report_t *report)
{
  term_puts("KB rep: ");
  term_puts("Mod=");
  term_puthex(report->modifier, 0);
  term_puts(" Keys=[");
  for (int i = 0; i < 6; i++)
  {
    term_puthex(report->keycode[i], 0);
    term_puts(" ");
  }
  term_puts("]\n");
}

int test_usb_device(int spi_id)
{
  int version;
  usb_device_info_t usb_device;
  hid_keyboard_report_t kb_report;
  int i;
  int result;

  term_puts("Initializing USB host...\n");
  if (!ch376_host_init(spi_id))
  {
    term_set_palette(PALETTE_RED_ON_BLACK);
    term_puts(" ERROR: Host init failed!\n");
    term_set_palette(PALETTE_WHITE_ON_BLACK);
    return 0;
  }
  term_set_palette(PALETTE_GREEN_ON_BLACK);
  term_puts(" USB host initialized\n");
  term_set_palette(PALETTE_WHITE_ON_BLACK);

  term_puts("Chip version: ");
  version = ch376_get_version(spi_id);
  term_putint(version);
  term_putchar('\n');

  term_puts("Checking for USB device...\n");
  result = ch376_test_connect(spi_id);
  if (result == CH376_CONN_DISCONNECTED)
  {
    term_set_palette(PALETTE_YELLOW_ON_BLACK);
    term_puts(" No device connected\n");
    term_set_palette(PALETTE_WHITE_ON_BLACK);
    return 1;
  }
  else if (result == CH376_CONN_CONNECTED)
  {
    term_set_palette(PALETTE_GREEN_ON_BLACK);
    term_puts(" Device connected (not initialized)\n");
    term_set_palette(PALETTE_WHITE_ON_BLACK);
  }
  else
  {
    term_set_palette(PALETTE_GREEN_ON_BLACK);
    term_puts(" Device ready\n");
    term_set_palette(PALETTE_WHITE_ON_BLACK);
  }

  term_puts("Enumerating USB device...\n");
  if (!ch376_enumerate_device(spi_id, &usb_device))
  {
    term_set_palette(PALETTE_RED_ON_BLACK);
    term_puts(" Enumeration failed!\n");
    term_set_palette(PALETTE_WHITE_ON_BLACK);
    return 0;
  }
  term_set_palette(PALETTE_GREEN_ON_BLACK);
  term_puts(" Device enumerated\n");
  term_putchar('\n');
  term_set_palette(PALETTE_WHITE_ON_BLACK);

  /* Print device info */
  print_device_info(&usb_device);

  if (ch376_is_keyboard(&usb_device))
  {
    term_puts("Keyboard detected! Polling...\n");

    // Poll keyboard
    while (1)
    {
      result = ch376_read_keyboard(spi_id, &usb_device, &kb_report);

      if (result == 1)
      {
        print_kb_report(&kb_report);
      }
      else if (result < 0)
      {
        term_puts("\nRead error! Status: ");
        term_puthex(-result, 1);
        term_putchar('\n');
        break;
      }
      else
      {
        /* NAK no new data */
      }

      /* Small delay between polls */
      delay(10);
    }
    term_putchar('\n');
  }

  return 1;
}

int main()
{
  init();
  term_puts("===== CH376 USB Host Library Test =====\n");

  term_puts("Testing bottom CH376 port\n");
  test_usb_device(CH376_SPI_BOTTOM);
  term_puts("Testing top CH376 port\n");
  test_usb_device(CH376_SPI_TOP);
  return 1;
}

void interrupt()
{
  int int_id = get_int_id();
  switch (int_id)
  {
  case INTID_TIMER2:
    timer_isr_handler(TIMER_2);
    break;
  default:
    break;
  }
}
