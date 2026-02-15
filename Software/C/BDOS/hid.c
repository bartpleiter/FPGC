/*
 * BDOS Human Interface Device (HID) module.
 * This module implements the following:
 * - USB keyboard driver
 * - HID subsystem for exposing input devices to BDOS
 */

#include "BDOS/bdos.h"

#define BDOS_KEY_EVENT_FIFO_SIZE 64
#define BDOS_KEY_REPEAT_DELAY_US 400000U
#define BDOS_KEY_REPEAT_INTERVAL_US 80000U

hid_keyboard_report_t bdos_prev_kb_report;
int bdos_keyboard_event_fifo[BDOS_KEY_EVENT_FIFO_SIZE];
int bdos_keyboard_event_fifo_head;
int bdos_keyboard_event_fifo_tail;
int bdos_repeat_keycode;
int bdos_repeat_modifier;
int bdos_repeat_event;
unsigned int bdos_repeat_start_us;
unsigned int bdos_repeat_last_us;

int bdos_keyboard_event_fifo_count()
{
  int head = bdos_keyboard_event_fifo_head;
  int tail = bdos_keyboard_event_fifo_tail;

  if (head >= tail)
  {
    return head - tail;
  }
  return BDOS_KEY_EVENT_FIFO_SIZE - tail + head;
}

int bdos_keyboard_event_fifo_push(int key_event)
{
  int next_head = (bdos_keyboard_event_fifo_head + 1) % BDOS_KEY_EVENT_FIFO_SIZE;

  if (next_head == bdos_keyboard_event_fifo_tail)
  {
    uart_puts("[BDOS] HID event FIFO full\n");
    return 0;
  }

  bdos_keyboard_event_fifo[bdos_keyboard_event_fifo_head] = key_event;
  bdos_keyboard_event_fifo_head = next_head;
  return 1;
}

int bdos_keyboard_event_available()
{
  return bdos_keyboard_event_fifo_count();
}

int bdos_keyboard_event_read()
{
  int key_event;

  if (bdos_keyboard_event_fifo_head == bdos_keyboard_event_fifo_tail)
  {
    return -1;
  }

  key_event = bdos_keyboard_event_fifo[bdos_keyboard_event_fifo_tail];
  bdos_keyboard_event_fifo_tail = (bdos_keyboard_event_fifo_tail + 1) % BDOS_KEY_EVENT_FIFO_SIZE;
  return key_event;
}

int bdos_keycode_in_report(const hid_keyboard_report_t *report, int keycode)
{
  int i;

  for (i = 0; i < 6; i++)
  {
    if (report->keycode[i] == keycode)
    {
      return 1;
    }
  }
  return 0;
}

int bdos_translate_key_event(int keycode, int modifier)
{
  int ascii;
  int ctrl_held = modifier & (USB_HID_MOD_LCTRL | USB_HID_MOD_RCTRL);

  if (ctrl_held && keycode >= 0x04 && keycode <= 0x1D)
  {
    return keycode - 0x03;
  }

  ascii = ch376_keycode_to_ascii(keycode, modifier);
  if (ascii)
  {
    return ascii & 0xFF;
  }

  switch (keycode)
  {
    case 0x3A: return BDOS_KEY_F1;
    case 0x3B: return BDOS_KEY_F2;
    case 0x3C: return BDOS_KEY_F3;
    case 0x3D: return BDOS_KEY_F4;
    case 0x3E: return BDOS_KEY_F5;
    case 0x3F: return BDOS_KEY_F6;
    case 0x40: return BDOS_KEY_F7;
    case 0x41: return BDOS_KEY_F8;
    case 0x42: return BDOS_KEY_F9;
    case 0x43: return BDOS_KEY_F10;
    case 0x44: return BDOS_KEY_F11;
    case 0x45: return BDOS_KEY_F12;
    case 0x49: return BDOS_KEY_INSERT;
    case 0x4A: return BDOS_KEY_HOME;
    case 0x4B: return BDOS_KEY_PAGEUP;
    case 0x4C: return BDOS_KEY_DELETE;
    case 0x4D: return BDOS_KEY_END;
    case 0x4E: return BDOS_KEY_PAGEDOWN;
    case 0x4F: return BDOS_KEY_RIGHT;
    case 0x50: return BDOS_KEY_LEFT;
    case 0x51: return BDOS_KEY_DOWN;
    case 0x52: return BDOS_KEY_UP;
    default: return 0;
  }
}

void bdos_clear_repeat_state()
{
  bdos_repeat_keycode = 0;
  bdos_repeat_modifier = 0;
  bdos_repeat_event = 0;
  bdos_repeat_start_us = 0;
  bdos_repeat_last_us = 0;
}

void bdos_start_repeat_state(int keycode, int modifier, int key_event, unsigned int now)
{
  bdos_repeat_keycode = keycode;
  bdos_repeat_modifier = modifier;
  bdos_repeat_event = key_event;
  bdos_repeat_start_us = now;
  bdos_repeat_last_us = now;
}

int bdos_find_new_keycode(const hid_keyboard_report_t *prev, const hid_keyboard_report_t *curr)
{
  int i;
  int keycode;

  for (i = 0; i < 6; i++)
  {
    keycode = curr->keycode[i];
    if (keycode == 0)
    {
      continue;
    }
    if (!bdos_keycode_in_report(prev, keycode))
    {
      return keycode;
    }
  }
  return 0;
}

void bdos_reset_keyboard_state()
{
  memset(&bdos_prev_kb_report, 0, sizeof(hid_keyboard_report_t));
  bdos_keyboard_event_fifo_head = 0;
  bdos_keyboard_event_fifo_tail = 0;
  bdos_clear_repeat_state();
  memset(&bdos_usb_keyboard_device, 0, sizeof(usb_device_info_t));
}

// To run during timer interrupt routine
void bdos_poll_usb_keyboard(int timer_id)
{
  hid_keyboard_report_t kb_report;
  hid_keyboard_report_t *active_report;
  int read_status;
  int new_keycode;
  int key_event;
  unsigned int now;
  
  // Check if we can poll for keyboard input
  if (bdos_usb_keyboard_device.connected)
  {
    if (ch376_test_connect(bdos_usb_keyboard_spi_id) == CH376_CONN_READY)
    {
      if (ch376_is_keyboard(&bdos_usb_keyboard_device))
      {
        now = get_micros();
        read_status = ch376_read_keyboard(bdos_usb_keyboard_spi_id, &bdos_usb_keyboard_device, &kb_report);

        if (read_status == 1)
        {
          // Successfully read a keyboard report, process only newly pressed key
          new_keycode = bdos_find_new_keycode(&bdos_prev_kb_report, &kb_report);

          if (new_keycode)
          {
            key_event = bdos_translate_key_event(new_keycode, kb_report.modifier);
            if (key_event)
            {
              bdos_keyboard_event_fifo_push(key_event);
            }
            bdos_start_repeat_state(new_keycode, kb_report.modifier, key_event, now);
          }

          memcpy(&bdos_prev_kb_report, &kb_report, sizeof(hid_keyboard_report_t));
        }

        if (bdos_repeat_keycode)
        {
          if (read_status == 1)
          {
            active_report = &kb_report;
          }
          else
          {
            active_report = &bdos_prev_kb_report;
          }

          if (!bdos_keycode_in_report(active_report, bdos_repeat_keycode))
          {
            bdos_clear_repeat_state();
          }
          else
          {
            if (active_report->modifier != bdos_repeat_modifier)
            {
              key_event = bdos_translate_key_event(bdos_repeat_keycode, active_report->modifier);
              bdos_start_repeat_state(bdos_repeat_keycode, active_report->modifier, key_event, now);
            }

            if (bdos_repeat_event &&
                (unsigned int)(now - bdos_repeat_start_us) >= BDOS_KEY_REPEAT_DELAY_US &&
                (unsigned int)(now - bdos_repeat_last_us) >= BDOS_KEY_REPEAT_INTERVAL_US)
            {
              if (bdos_keyboard_event_fifo_push(bdos_repeat_event))
              {
                bdos_repeat_last_us = now;
              }
            }
          }
        }
      }
    }
  }
}

// To run within the main loop of BDOS after initialization
void bdos_usb_keyboard_main_loop()
{
  // Check device status
  int status = ch376_test_connect(bdos_usb_keyboard_spi_id);
  if (status == CH376_CONN_DISCONNECTED)
  {
    if (bdos_usb_keyboard_device.connected)
    {
      // Device was previously connected, now disconnected
      bdos_reset_keyboard_state();
      // Sadly the below does not prevent the CH376 from getting into a non-functional state that only can be solved by a power cycle
      ch376_reset(bdos_usb_keyboard_spi_id);
      bdos_init_usb_keyboard(); // Re-initialize to be ready for next connection
      uart_puts("[BDOS] USB keyboard disconnected\n");
    }
  }
  if (status == CH376_CONN_CONNECTED)
  {
    // Device connected, but not initialized yet
    if (ch376_enumerate_device(bdos_usb_keyboard_spi_id, &bdos_usb_keyboard_device))
    {
      uart_puts("[BDOS] USB keyboard connected and enumerated!\n");
    }
    else
    {
      uart_puts("[BDOS] Failed to enumerate USB keyboard\n");
    }
  }
}
