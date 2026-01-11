#define COMMON_STDLIB
#define COMMON_STRING
#include "libs/common/common.h"

#define KERNEL_TIMER
#define KERNEL_CH376
#define KERNEL_UART
#include "libs/kernel/kernel.h"

/* Print device descriptor info */
void print_device_info(usb_device_info_t *info)
{
  uart_puts("USB Device Info:\n");
  uart_puts("  VID: ");
  uart_puthex(info->device_desc.idVendor, 1);
  uart_puts(" PID: ");
  uart_puthex(info->device_desc.idProduct, 1);
  uart_putchar('\n');

  uart_puts("  Class: ");
  uart_puthex(info->device_desc.bDeviceClass, 1);
  uart_puts(" SubClass: ");
  uart_puthex(info->device_desc.bDeviceSubClass, 1);
  uart_putchar('\n');

  uart_puts("  Interface Class: ");
  uart_puthex(info->interface_class, 1);
  uart_puts(" SubClass: ");
  uart_puthex(info->interface_subclass, 1);
  uart_puts(" Protocol: ");
  uart_puthex(info->interface_protocol, 1);
  uart_putchar('\n');

  if (info->low_speed)
  {
    uart_puts("  Speed: Low (1.5Mbps)\n");
  }
  else
  {
    uart_puts("  Speed: Full (12Mbps)\n");
  }

  if (ch376_is_keyboard(info))
  {
    uart_puts("  Type: HID Keyboard\n");
    uart_puts("  Interrupt EP: ");
    uart_puthex(info->interrupt_endpoint, 1);
    uart_putchar('\n');
  }
  else if (ch376_is_mouse(info))
  {
    uart_puts("  Type: HID Mouse\n");
  }
  else
  {
    uart_puts("  Type: Unknown\n");
  }
}

void print_kb_report(hid_keyboard_report_t *report)
{
  uart_puts("Keyboard Report:\n");
  uart_puts("  Modifier: ");
  uart_puthex(report->modifier, 1);
  uart_putchar('\n');
  uart_puts("  Keycodes: ");
  for (int i = 0; i < 6; i++)
  {
    uart_puthex(report->keycode[i], 1);
    uart_puts(" ");
  }
  uart_putchar('\n');
}

int test_usb_device(int spi_id)
{
  int version;
  usb_device_info_t usb_device;
  hid_keyboard_report_t kb_report;
  int i;
  int result;

  /* Test 1: Initialize as USB host */
  uart_puts("1. Initializing USB host...\n");
  if (!ch376_host_init(spi_id))
  {
    uart_puts("   ERROR: Host init failed!\n");
    return 0;
  }
  uart_puts("   OK: USB host initialized\n");

  /* Test 2: Get version */
  uart_puts("2. Getting chip version...\n");
  version = ch376_get_version(spi_id);
  uart_puts("   Version: ");
  uart_putint(version);
  uart_putchar('\n');

  /* Test 3: Check connection status */
  uart_puts("3. Checking for USB device...\n");
  result = ch376_test_connect(spi_id);
  if (result == CH376_CONN_DISCONNECTED)
  {
    uart_puts("   No device connected\n");
    uart_puts("\nPlease connect a USB device.\n");
    return 1;
  }
  else if (result == CH376_CONN_CONNECTED)
  {
    uart_puts("   Device connected (not initialized)\n");
  }
  else
  {
    uart_puts("   Device ready\n");
  }

  /* Test 4: Enumerate device using library function */
  uart_puts("4. Enumerating USB device...\n");
  if (!ch376_enumerate_device(spi_id, &usb_device))
  {
    uart_puts("   ERROR: Enumeration failed!\n");
    return 0;
  }
  uart_puts("   OK: Device enumerated\n");
  uart_putchar('\n');

  /* Print device info */
  print_device_info(&usb_device);

  if (ch376_is_keyboard(&usb_device))
  {
    // Test 5: Read keyboard input
    uart_puts("\n5. Keyboard detected! Polling for keypresses...\n");
    uart_puts("   (Press some keys on the USB keyboard)\n\n");

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
        uart_puts("\nRead error! Status: ");
        uart_puthex(-result, 1);
        uart_putchar('\n');
        break;
      }
      else
      {
        /* NAK no new data */
      }
      
      /* Small delay between polls */
      delay(10);
    }
    uart_putchar('\n');
  }

  return 1;
}

int main()
{
  uart_puts("\n=== CH376 USB Host Library Test ===\n\n");

  uart_puts("\n\nTesting bottom CH376 port (with USB keyboard)...\n");
  test_usb_device(CH376_SPI_BOTTOM);
  uart_puts("\n=== Test Complete ===\n");
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
