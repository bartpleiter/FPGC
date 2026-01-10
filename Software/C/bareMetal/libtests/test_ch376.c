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

int test_usb_device(int spi_id)
{
  int version;
  usb_device_info_t usb_device;
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
    uart_puts("\n5. Keyboard detected!\n");
  }

  return 1;
}

int main()
{
  uart_puts("\n=== CH376 USB Host Library Test ===\n\n");

  // uart_puts("Testing top CH376 port...\n");
  // test_usb_device(CH376_SPI_TOP);

  uart_puts("\n\nTesting bottom CH376 port...\n");
  test_usb_device(CH376_SPI_BOTTOM);

  // /* Test 5: If keyboard, try reading keys */
  // if (ch376_is_keyboard(&usb_device))
  // {
  //     uart_puts("\n5. Keyboard detected! Setting up...\n");

  //     /* Set boot protocol for simpler reports */
  //     ch376_hid_set_protocol(spi_id, &usb_device, 0);

  //     /* Set idle to 0 (only report on change) */
  //     ch376_hid_set_idle(spi_id, &usb_device, 0);

  //     uart_puts("   Press keys (showing first 10):\n   ");

  //     /* Try reading a few key presses */
  //     while (key_count < 10)
  //     {
  //         result = ch376_read_keyboard(spi_id, &usb_device, &kb_report);

  //         if (result == 1)
  //         {
  //             /* Check for any key pressed */
  //             for (i = 0; i < 6; i++)
  //             {
  //                 if (kb_report.keycode[i] != 0)
  //                 {
  //                     char c = ch376_keycode_to_ascii(kb_report.keycode[i], kb_report.modifier);
  //                     if (c != 0)
  //                     {
  //                         uart_putchar(c);
  //                         key_count++;
  //                     }
  //                     else
  //                     {
  //                         uart_puts("[");
  //                         uart_puthex(kb_report.keycode[i], 1);
  //                         uart_puts("]");
  //                         key_count++;
  //                     }
  //                 }
  //             }
  //         }
  //         else if (result < 0)
  //         {
  //             uart_puts("\n   Read error! Status: ");
  //             uart_puthex(-result, 1);
  //             uart_putchar('\n');
  //             break;
  //         }

  //         delay(10);
  //     }
  //     uart_puts("\n");
  // }

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
