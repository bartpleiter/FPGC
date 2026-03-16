/*
 * hid.c — BDOS v3 Human Interface Device (HID) module.
 *
 * USB keyboard driver and input event pipeline.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "bdos.h"

#define BDOS_KEY_EVENT_FIFO_SIZE 64
#define BDOS_KEY_REPEAT_DELAY_US 400000U
#define BDOS_KEY_REPEAT_INTERVAL_US 80000U

int bdos_usb_keyboard_spi_id = CH376_SPI_BOTTOM;
usb_device_info_t bdos_usb_keyboard_device;
unsigned int bdos_key_state_bitmap = 0;

static hid_keyboard_report_t bdos_prev_kb_report;
static int bdos_keyboard_event_fifo[BDOS_KEY_EVENT_FIFO_SIZE];
static int bdos_keyboard_event_fifo_head;
static int bdos_keyboard_event_fifo_tail;
static int bdos_repeat_keycode;
static int bdos_repeat_modifier;
static int bdos_repeat_event;
static unsigned int bdos_repeat_start_us;
static unsigned int bdos_repeat_last_us;

static int bdos_keyboard_event_fifo_count(void)
{
  int head;
  int tail;

  head = bdos_keyboard_event_fifo_head;
  tail = bdos_keyboard_event_fifo_tail;

  if (head >= tail)
  {
    return head - tail;
  }
  return BDOS_KEY_EVENT_FIFO_SIZE - tail + head;
}

int bdos_keyboard_event_fifo_push(int key_event)
{
  int next_head;

  next_head = (bdos_keyboard_event_fifo_head + 1) % BDOS_KEY_EVENT_FIFO_SIZE;

  if (next_head == bdos_keyboard_event_fifo_tail)
  {
    uart_puts("[BDOS] HID event FIFO full\n");
    return 0;
  }

  bdos_keyboard_event_fifo[bdos_keyboard_event_fifo_head] = key_event;
  bdos_keyboard_event_fifo_head = next_head;
  return 1;
}

int bdos_keyboard_event_available(void)
{
  return bdos_keyboard_event_fifo_count();
}

int bdos_keyboard_event_read(void)
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

static int bdos_keycode_in_report(const hid_keyboard_report_t *report, int keycode)
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

static int bdos_translate_key_event(int keycode, int modifier)
{
  int ascii;
  int ctrl_held;

  ctrl_held = modifier & (USB_HID_MOD_LCTRL | USB_HID_MOD_RCTRL);

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

static void bdos_clear_repeat_state(void)
{
  bdos_repeat_keycode = 0;
  bdos_repeat_modifier = 0;
  bdos_repeat_event = 0;
  bdos_repeat_start_us = 0;
  bdos_repeat_last_us = 0;
}

static void bdos_start_repeat_state(int keycode, int modifier, int key_event, unsigned int now)
{
  bdos_repeat_keycode = keycode;
  bdos_repeat_modifier = modifier;
  bdos_repeat_event = key_event;
  bdos_repeat_start_us = now;
  bdos_repeat_last_us = now;
}

static int bdos_find_new_keycode(const hid_keyboard_report_t *prev, const hid_keyboard_report_t *curr)
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

void bdos_reset_keyboard_state(void)
{
  memset(&bdos_prev_kb_report, 0, sizeof(hid_keyboard_report_t));
  bdos_keyboard_event_fifo_head = 0;
  bdos_keyboard_event_fifo_tail = 0;
  bdos_clear_repeat_state();
  memset(&bdos_usb_keyboard_device, 0, sizeof(usb_device_info_t));
  bdos_key_state_bitmap = 0;
}

void bdos_rebuild_key_state_bitmap(const hid_keyboard_report_t *report)
{
  unsigned int bitmap;
  int i;
  int kc;

  bitmap = 0;

  if (report->modifier & (USB_HID_MOD_LSHIFT | USB_HID_MOD_RSHIFT))
    bitmap |= KEYSTATE_SHIFT;
  if (report->modifier & (USB_HID_MOD_LCTRL | USB_HID_MOD_RCTRL))
    bitmap |= KEYSTATE_CTRL;

  for (i = 0; i < 6; i++)
  {
    kc = report->keycode[i];
    if (kc == 0)
      continue;

    if (kc == 0x04) bitmap |= KEYSTATE_A;
    else if (kc == 0x07) bitmap |= KEYSTATE_D;
    else if (kc == 0x08) bitmap |= KEYSTATE_E;
    else if (kc == 0x14) bitmap |= KEYSTATE_Q;
    else if (kc == 0x16) bitmap |= KEYSTATE_S;
    else if (kc == 0x1A) bitmap |= KEYSTATE_W;
    else if (kc == 0x52) bitmap |= KEYSTATE_UP;
    else if (kc == 0x51) bitmap |= KEYSTATE_DOWN;
    else if (kc == 0x50) bitmap |= KEYSTATE_LEFT;
    else if (kc == 0x4F) bitmap |= KEYSTATE_RIGHT;
    else if (kc == 0x2C) bitmap |= KEYSTATE_SPACE;
    else if (kc == 0x29) bitmap |= KEYSTATE_ESCAPE;
  }

  bdos_key_state_bitmap = bitmap;
}

void bdos_poll_usb_keyboard(int timer_id)
{
  hid_keyboard_report_t kb_report;
  hid_keyboard_report_t *active_report;
  int read_status;
  int new_keycode;
  int key_event;
  unsigned int now;

  (void)timer_id;

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
          new_keycode = bdos_find_new_keycode(&bdos_prev_kb_report, &kb_report);

          bdos_rebuild_key_state_bitmap(&kb_report);

          if (new_keycode)
          {
            key_event = bdos_translate_key_event(new_keycode, kb_report.modifier);
            if (key_event)
            {
              if (bdos_active_slot != BDOS_SLOT_NONE)
              {
                if (key_event == BDOS_KEY_F4 &&
                    (kb_report.modifier & (USB_HID_MOD_LALT | USB_HID_MOD_RALT)))
                {
                  bdos_kill_requested = 1;
                }
                else if (key_event == BDOS_KEY_F12)
                {
                  bdos_switch_target = -2;
                }
                else if (key_event >= BDOS_KEY_F1 && key_event <= BDOS_KEY_F8)
                {
                  int target_slot;
                  target_slot = key_event - BDOS_KEY_F1;
                  if (target_slot != bdos_active_slot &&
                      target_slot < MEM_SLOT_COUNT &&
                      bdos_slot_status[target_slot] == BDOS_SLOT_STATUS_SUSPENDED)
                  {
                    bdos_switch_target = target_slot;
                  }
                }
                else
                {
                  bdos_keyboard_event_fifo_push(key_event);
                }
              }
              else
              {
                if ((kb_report.modifier & (USB_HID_MOD_LCTRL | USB_HID_MOD_RCTRL)) &&
                    (key_event == BDOS_KEY_UP || key_event == BDOS_KEY_DOWN))
                {
                  /* Don't push to FIFO; scrollback handled via bitmap */
                }
                else
                {
                  bdos_keyboard_event_fifo_push(key_event);
                }
              }
            }
            bdos_start_repeat_state(new_keycode, kb_report.modifier, key_event, now);
          }

          memcpy(&bdos_prev_kb_report, &kb_report, sizeof(hid_keyboard_report_t));
        }
        else
        {
          bdos_rebuild_key_state_bitmap(&bdos_prev_kb_report);
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
              int ctrl_in_report;
              ctrl_in_report = active_report->modifier & (USB_HID_MOD_LCTRL | USB_HID_MOD_RCTRL);
              if (ctrl_in_report &&
                  (bdos_repeat_event == BDOS_KEY_UP || bdos_repeat_event == BDOS_KEY_DOWN))
              {
                bdos_repeat_last_us = now;
              }
              else if (bdos_keyboard_event_fifo_push(bdos_repeat_event))
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

void bdos_usb_keyboard_main_loop(void)
{
  int status;

  status = ch376_test_connect(bdos_usb_keyboard_spi_id);
  if (status == CH376_CONN_DISCONNECTED)
  {
    if (bdos_usb_keyboard_device.connected)
    {
      bdos_reset_keyboard_state();
      ch376_reset(bdos_usb_keyboard_spi_id);
      ch376_host_init(bdos_usb_keyboard_spi_id);
      uart_puts("[BDOS] USB keyboard disconnected\n");
    }
  }
  if (status == CH376_CONN_CONNECTED)
  {
    delay(1000);

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
