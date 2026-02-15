#include "libs/kernel/io/ch376.h"
#include "libs/kernel/io/spi.h"

/* ============================================ */
/*          Internal Helper Functions           */
/* ============================================ */

/* Send a command byte to CH376 */
static void ch376_send_cmd(int spi_id, int cmd)
{
  spi_select(spi_id);
  spi_transfer(spi_id, cmd);

  // Wait for 2uS
  int start = get_micros();
  while ((get_micros() - start) < 2);
}

/* End command (deselect) */
static void ch376_end_cmd(int spi_id)
{
  spi_deselect(spi_id);
}

/* ============================================ */
/*          Interrupt Pin Reading               */
/* ============================================ */

int ch376_get_top_nint()
{
  int retval = 0;
  asm(
      "load32 0x700000E r11 ; r11 = nint of top CH376"
      "read 0 r11 r11       ; Read nint pin state"
      "write -1 r14 r11     ; Write to stack for return");
  return retval;
}

int ch376_get_bottom_nint()
{
  int retval = 0;
  asm(
      "load32 0x7000011 r11 ; r11 = nint of bottom CH376"
      "read 0 r11 r11       ; Read nint pin state"
      "write -1 r14 r11     ; Write to stack for return");
  return retval;
}

int ch376_read_int(int spi_id)
{
  /* INT# is active low, so we invert the logic */
  if (spi_id == CH376_SPI_TOP)
  {
    return ch376_get_top_nint() == 0 ? 1 : 0;
  }
  if (spi_id == CH376_SPI_BOTTOM)
  {
    return ch376_get_bottom_nint() == 0 ? 1 : 0;
  }
  return 0;
}

/* ============================================ */
/*          Basic CH376 Functions               */
/* ============================================ */

int ch376_get_version(int spi_id)
{
  int version;
  ch376_send_cmd(spi_id, CH376_CMD_GET_IC_VER);
  version = spi_transfer(spi_id, 0x00);
  ch376_end_cmd(spi_id);
  /* Return bits 5-0 as version number */
  return version & 0x3F;
}

int ch376_check_exist(int spi_id)
{
  int test_val = 0x57;
  int result;

  spi_deselect(spi_id);

  ch376_send_cmd(spi_id, CH376_CMD_CHECK_EXIST);
  spi_transfer(spi_id, test_val);

  result = spi_transfer(spi_id, 0x00);
  ch376_end_cmd(spi_id);

  /* Should return bitwise NOT of input */
  return (result == (test_val ^ 0xFF)) ? 1 : 0;
}

void ch376_reset(int spi_id)
{
  ch376_send_cmd(spi_id, CH376_CMD_RESET_ALL);
  ch376_end_cmd(spi_id);
  /* Wait for reset to complete*/
  delay(100);
}

int ch376_set_usb_mode(int spi_id, int mode)
{
  int status;

  ch376_send_cmd(spi_id, CH376_CMD_SET_USB_MODE);
  spi_transfer(spi_id, mode);

  // Wait a long time for the mode to take effect
  delay(10);

  status = spi_transfer(spi_id, 0x00);
  ch376_end_cmd(spi_id);

  return status;
}

void ch376_set_usb_speed(int spi_id, int speed)
{
  ch376_send_cmd(spi_id, CH376_CMD_SET_USB_SPEED);
  spi_transfer(spi_id, speed);
  ch376_end_cmd(spi_id);
}

int ch376_get_status(int spi_id)
{
  int status;

  ch376_send_cmd(spi_id, CH376_CMD_GET_STATUS);
  status = spi_transfer(spi_id, 0x00);
  ch376_end_cmd(spi_id);

  return status;
}

int ch376_wait_interrupt(int spi_id, int timeout_ms)
{
  int now = 0;
  int start = get_micros();
  int i;

  while (1)
  {
    if (ch376_read_int(spi_id))
    {
      return ch376_get_status(spi_id);
    }
    
    now = get_micros();
    if ((now - start) >= (timeout_ms * 1000))
    {
      return -1; /* Timeout */
    }

    /* Small delay to avoid busy waiting too tightly */
    for (i = 0; i < 100; i++);
  }
  return -1; // Should not reach here
}

int ch376_test_connect(int spi_id)
{
  int status;

  ch376_send_cmd(spi_id, CH376_CMD_TEST_CONNECT);
  status = spi_transfer(spi_id, 0x00);
  ch376_end_cmd(spi_id);

  if (status == CH376_INT_CONNECT)
  {
    return CH376_CONN_CONNECTED;
  }
  else if (status == CH376_INT_USB_READY)
  {
    return CH376_CONN_READY;
  }
  else if (status == CH376_INT_DISCONNECT)
  {
    return CH376_CONN_DISCONNECTED;
  }
  return CH376_CONN_UNKNOWN;
}

void ch376_set_retry(int spi_id, int retry)
{
  ch376_send_cmd(spi_id, CH376_CMD_SET_RETRY);
  spi_transfer(spi_id, 0x25);
  spi_transfer(spi_id, retry);
  ch376_end_cmd(spi_id);
}

int ch376_read_data(int spi_id, char *buffer, int max_len)
{
  int len;
  int i;

  ch376_send_cmd(spi_id, CH376_CMD_RD_USB_DATA0);
  len = spi_transfer(spi_id, 0x00);

  if (len > max_len)
  {
    len = max_len;
  }

  for (i = 0; i < len; i++)
  {
    buffer[i] = spi_transfer(spi_id, 0x00);
  }

  ch376_end_cmd(spi_id);
  return len;
}

void ch376_write_data(int spi_id, char *buffer, int len)
{
  int i;

  ch376_send_cmd(spi_id, CH376_CMD_WR_HOST_DATA);
  spi_transfer(spi_id, len);

  for (i = 0; i < len; i++)
  {
    spi_transfer(spi_id, buffer[i]);
  }

  ch376_end_cmd(spi_id);
}

/* ============================================ */
/*          USB Host Functions                  */
/* ============================================ */

int ch376_host_init(int spi_id)
{
  /* Ensure clean state */
  spi_deselect(spi_id);
  delay(10);

  /* Reset the chip first */
  ch376_reset(spi_id);

  /* Check if chip exists */
  if (!ch376_check_exist(spi_id))
  {
    return 0;
  }

  /* Set to USB host mode (not generating SOF) */
  ch376_set_usb_mode(spi_id, CH376_MODE_HOST_ENABLED);

  return 1;
}

int ch376_detect_device(int spi_id, int *is_low_speed)
{
  int rate;
  int conn_status;

  /* Check connection */
  conn_status = ch376_test_connect(spi_id);
  if (conn_status == CH376_CONN_DISCONNECTED)
  {
    *is_low_speed = 0;
    return 0;
  }

  /* Get device rate */
  ch376_send_cmd(spi_id, CH376_CMD_GET_DEV_RATE);
  spi_transfer(spi_id, 0x07);
  rate = spi_transfer(spi_id, 0x00);
  ch376_end_cmd(spi_id);

  *is_low_speed = (rate & 0x10) ? 1 : 0;

  return 1;
}

void ch376_set_rx_toggle(int spi_id, int toggle)
{
  int mode;
  if (toggle)
  {
    mode = 0xC0;
  }
  else
  {
    mode = 0x80;
  }
  ch376_send_cmd(spi_id, CH376_CMD_SET_ENDP6);
  spi_transfer(spi_id, mode);
  ch376_end_cmd(spi_id);
}

void ch376_set_tx_toggle(int spi_id, int toggle)
{
  int mode = 0x80 | (toggle ? 0x40 : 0x00);
  ch376_send_cmd(spi_id, CH376_CMD_SET_ENDP7);
  spi_transfer(spi_id, mode);
  ch376_end_cmd(spi_id);
}

int ch376_issue_token(int spi_id, int endpoint, int pid)
{
  int transaction_attr;
  int status;

  transaction_attr = ((endpoint & 0x0F) << 4) | (pid & 0x0F);

  ch376_send_cmd(spi_id, CH376_CMD_ISSUE_TOKEN);
  spi_transfer(spi_id, transaction_attr);
  ch376_end_cmd(spi_id);
  
  /* Wait for transaction to complete */
  status = ch376_wait_interrupt(spi_id, 500);

  return status;
}

/* Issue token with explicit sync flags (CMD_ISSUE_TKN_X) */
/* sync_flags: bit 7 = RX sync toggle, bit 6 = TX sync toggle */
int ch376_issue_token_x(int spi_id, int sync_flags, int endpoint, int pid)
{
  int transaction_attr;
  int status;

  transaction_attr = ((endpoint & 0x0F) << 4) | (pid & 0x0F);

  ch376_send_cmd(spi_id, CH376_CMD_ISSUE_TKN_X);
  spi_transfer(spi_id, sync_flags);
  spi_transfer(spi_id, transaction_attr);
  ch376_end_cmd(spi_id);
  
  /* Wait for transaction to complete */
  status = ch376_wait_interrupt(spi_id, 500);

  return status;
}

int ch376_get_device_descriptor(int spi_id, usb_device_descriptor_t *desc)
{
  int status;
  char buffer[18];
  int len;

  ch376_send_cmd(spi_id, CH376_CMD_GET_DESCR);
  spi_transfer(spi_id, CH376_DESCR_DEVICE);
  ch376_end_cmd(spi_id);

  status = ch376_wait_interrupt(spi_id, 500);
  if (status != CH376_INT_SUCCESS)
  {
    return 0;
  }

  len = ch376_read_data(spi_id, buffer, 18);
  if (len < 18)
  {
    return 0;
  }

  /* Parse device descriptor */
  desc->bLength = buffer[0] & 0xFF;
  desc->bDescriptorType = buffer[1] & 0xFF;
  desc->bcdUSB = ((buffer[3] & 0xFF) << 8) | (buffer[2] & 0xFF);
  desc->bDeviceClass = buffer[4] & 0xFF;
  desc->bDeviceSubClass = buffer[5] & 0xFF;
  desc->bDeviceProtocol = buffer[6] & 0xFF;
  desc->bMaxPacketSize0 = buffer[7] & 0xFF;
  desc->idVendor = ((buffer[9] & 0xFF) << 8) | (buffer[8] & 0xFF);
  desc->idProduct = ((buffer[11] & 0xFF) << 8) | (buffer[10] & 0xFF);
  desc->bcdDevice = ((buffer[13] & 0xFF) << 8) | (buffer[12] & 0xFF);
  desc->iManufacturer = buffer[14] & 0xFF;
  desc->iProduct = buffer[15] & 0xFF;
  desc->iSerialNumber = buffer[16] & 0xFF;
  desc->bNumConfigurations = buffer[17] & 0xFF;

  return 1;
}

int ch376_set_device_address(int spi_id, int address)
{
  int status;

  ch376_send_cmd(spi_id, CH376_CMD_SET_ADDRESS);
  spi_transfer(spi_id, address);
  ch376_end_cmd(spi_id);

  status = ch376_wait_interrupt(spi_id, 500);
  if (status != CH376_INT_SUCCESS)
  {
    return 0;
  }

  /* Also set the internal USB address for future communications */
  ch376_send_cmd(spi_id, CH376_CMD_SET_USB_ADDR);
  spi_transfer(spi_id, address);
  ch376_end_cmd(spi_id);

  return 1;
}

int ch376_set_device_config(int spi_id, int config)
{
  int status;

  ch376_send_cmd(spi_id, CH376_CMD_SET_CONFIG);
  spi_transfer(spi_id, config);
  ch376_end_cmd(spi_id);

  status = ch376_wait_interrupt(spi_id, 500);
  if (status == CH376_INT_SUCCESS)
  {
    return 1;
  }
  return 0;
}

/* ============================================ */
/*          High-Level USB Functions            */
/* ============================================ */

/* Parse configuration descriptor to find HID interface and interrupt endpoint */
static int parse_config_descriptor(char *buffer, int len, usb_device_info_t *info)
{
  int offset = 0;
  int desc_len;
  int desc_type;
  int current_interface_class = 0;
  int current_interface_subclass = 0;
  int current_interface_protocol = 0;
  int found_keyboard = 0;

  while (offset < len)
  {
    desc_len = buffer[offset] & 0xFF;
    if (desc_len == 0)
      break;
    if (offset + desc_len > len)
      break;

    desc_type = buffer[offset + 1] & 0xFF;

    if (desc_type == CH376_DESCR_INTERFACE && desc_len >= 9)
    {
      /* Interface descriptor */
      current_interface_class = buffer[offset + 5] & 0xFF;
      current_interface_subclass = buffer[offset + 6] & 0xFF;
      current_interface_protocol = buffer[offset + 7] & 0xFF;

      /* Check if this is a HID boot keyboard */
      if (current_interface_class == USB_CLASS_HID &&
          current_interface_subclass == USB_HID_SUBCLASS_BOOT &&
          current_interface_protocol == USB_HID_PROTOCOL_KEYBOARD)
      {
        info->interface_class = current_interface_class;
        info->interface_subclass = current_interface_subclass;
        info->interface_protocol = current_interface_protocol;
        found_keyboard = 1;
      }
      /* If no keyboard found yet, save any HID interface */
      else if (!found_keyboard && current_interface_class == USB_CLASS_HID)
      {
        info->interface_class = current_interface_class;
        info->interface_subclass = current_interface_subclass;
        info->interface_protocol = current_interface_protocol;
      }
    }
    else if (desc_type == CH376_DESCR_ENDPOINT && desc_len >= 7)
    {
      /* Endpoint descriptor - save if for a keyboard interface or any HID if no keyboard */
      int ep_addr = buffer[offset + 2] & 0xFF;
      int ep_attr = buffer[offset + 3] & 0xFF;
      int ep_size = ((buffer[offset + 5] & 0xFF) << 8) | (buffer[offset + 4] & 0xFF);

      /* Check if this is an interrupt IN endpoint */
      if ((ep_addr & 0x80) && ((ep_attr & 0x03) == 0x03))
      {
        /* If we just found a keyboard interface, save this endpoint */
        if (found_keyboard && info->interrupt_endpoint == 0)
        {
          info->interrupt_endpoint = ep_addr;
          info->interrupt_max_packet = ep_size;
        }
        /* Save first interrupt endpoint if none saved yet */
        else if (info->interrupt_endpoint == 0)
        {
          info->interrupt_endpoint = ep_addr;
          info->interrupt_max_packet = ep_size;
        }
      }
    }

    offset += desc_len;
  }

  return (info->interface_class == USB_CLASS_HID) ? 1 : 0;
}

int ch376_enumerate_device(int spi_id, usb_device_info_t *info)
{
  int is_low_speed = 0;
  char config_buffer[64];
  int config_len;
  int status = 0;

  /* Clear the info structure */
  info->connected = 0;
  info->low_speed = 0;
  info->address = 0;
  info->interface_class = 0;
  info->interface_subclass = 0;
  info->interface_protocol = 0;
  info->interrupt_endpoint = 0;
  info->interrupt_max_packet = 8;
  info->toggle_in = 0;

  /* Check if device is connected */
  if (!ch376_detect_device(spi_id, &is_low_speed))
  {
    return 0;
  }

  info->low_speed = is_low_speed;

  /* Set appropriate speed */
  if (is_low_speed)
  {
    ch376_set_usb_speed(spi_id, CH376_SPEED_LOW);
  }
  else
  {
    ch376_set_usb_speed(spi_id, CH376_SPEED_FULL);
  }

  /* Reset USB bus for the device */
  ch376_set_usb_mode(spi_id, CH376_MODE_HOST_RESET);
  ch376_set_usb_mode(spi_id, CH376_MODE_HOST_SOF);

  /* Wait for device to connect again */
  status = ch376_wait_interrupt(spi_id, 2000);
  if (status != CH376_INT_CONNECT)
  {
    return 0;
  }

  /* Set speed again after mode change */
  if (is_low_speed)
  {
    ch376_set_usb_speed(spi_id, CH376_SPEED_LOW);
  }
  else
  {
    ch376_set_usb_speed(spi_id, CH376_SPEED_FULL);
  }

  /* Get device descriptor at address 0 */
  if (!ch376_get_device_descriptor(spi_id, &info->device_desc))
  {
    return 0;
  }

  /* Assign device address */
  if (!ch376_set_device_address(spi_id, 1))
  {
    return 0;
  }
  info->address = 1;

  /* Set configuration (use configuration 1) */
  if (!ch376_set_device_config(spi_id, 1))
  {
    return 0;
  }

  /* Get configuration descriptor */
  config_len = ch376_get_config_descriptor(spi_id, config_buffer, 64);

  if (config_len > 0)
  {
    parse_config_descriptor(config_buffer, config_len, info);
  }

  info->connected = 1;
  return 1;
}

int ch376_is_keyboard(usb_device_info_t *info)
{
  return (info->interface_class == USB_CLASS_HID &&
          info->interface_subclass == USB_HID_SUBCLASS_BOOT &&
          info->interface_protocol == USB_HID_PROTOCOL_KEYBOARD);
}

int ch376_is_mouse(usb_device_info_t *info)
{
  return (info->interface_class == USB_CLASS_HID &&
          info->interface_subclass == USB_HID_SUBCLASS_BOOT &&
          info->interface_protocol == USB_HID_PROTOCOL_MOUSE);
}

/* Send a USB control transfer (for standard GET_DESCRIPTOR requests) */
/* Handles multi-packet IN transfers for descriptors > 8 bytes */
static int ch376_control_transfer_in(int spi_id, char *setup, int setup_len,
                                     char *data_out, int max_data_len)
{
  int status;
  int total_len = 0;
  int chunk_len;
  int toggle = 1;                                 /* Start with DATA1 after SETUP */
  int requested_len = (setup[7] << 8) | setup[6]; /* wLength */

  /* Setup stage: send SETUP packet with DATA0 */
  ch376_set_tx_toggle(spi_id, 0);
  ch376_write_data(spi_id, setup, setup_len);
  status = ch376_issue_token(spi_id, 0x00, CH376_PID_SETUP);
  if (status != CH376_INT_SUCCESS)
  {
    return -1;
  }

  /* Data stage: receive data in multiple packets if needed */
  while (total_len < max_data_len && total_len < requested_len)
  {
    ch376_set_rx_toggle(spi_id, toggle);
    status = ch376_issue_token(spi_id, toggle ? 0x80 : 0x00, CH376_PID_IN);

    if (status != CH376_INT_SUCCESS)
    {
      /* If we got some data, that's OK */
      if (total_len > 0)
        break;
      return -1;
    }

    chunk_len = ch376_read_data(spi_id, data_out + total_len, max_data_len - total_len);
    if (chunk_len == 0)
    {
      break; /* No more data */
    }

    total_len += chunk_len;
    toggle = toggle ? 0 : 1; /* Toggle for next packet */

    /* If we got less than 8 bytes, that was the last packet */
    if (chunk_len < 8)
    {
      break;
    }
  }

  /* Status stage: send zero-length OUT with DATA1 */
  ch376_set_tx_toggle(spi_id, 1);
  ch376_write_data(spi_id, setup, 0); /* Zero length */
  status = ch376_issue_token(spi_id, 0x40, CH376_PID_OUT);
  /* Ignore status stage errors for now */

  return total_len;
}

/* Manual implementation of GET_DESCRIPTOR for config */
int ch376_get_config_descriptor(int spi_id, char *buffer, int max_len)
{
  char setup[8];
  int len;

  /* Build GET_DESCRIPTOR request for Config Descriptor */
  setup[0] = 0x80;           /* bmRequestType: Device to Host, Standard, Device */
  setup[1] = 0x06;           /* bRequest: GET_DESCRIPTOR */
  setup[2] = 0x00;           /* wValue low: descriptor index */
  setup[3] = 0x02;           /* wValue high: descriptor type (CONFIGURATION) */
  setup[4] = 0x00;           /* wIndex low */
  setup[5] = 0x00;           /* wIndex high */
  setup[6] = max_len & 0xFF; /* wLength low */
  setup[7] = 0x00;           /* wLength high */

  len = ch376_control_transfer_in(spi_id, setup, 8, buffer, max_len);

  return (len > 0) ? len : 0;
}


int ch376_read_keyboard(int spi_id, usb_device_info_t *info, hid_keyboard_report_t *report)
{
  int endpoint;
  int status;
  char buffer[64];
  int len;
  int i;
  int sync_flags;

  if (!info->connected || info->interrupt_endpoint == 0)
  {
    return -1;
  }

  /* Get endpoint number (remove direction bit) */
  endpoint = info->interrupt_endpoint & 0x0F;

  /* Set retry behavior: no infinite NAK retry, limited timeout retries */
  /* Bits 7-6 = 00: NAK notified as result, Bits 5-0 = 0x0F: 15 timeout retries */
  ch376_set_retry(spi_id, 0x0F);

  /* Build sync flags: bit 7 = RX toggle, bit 6 = TX toggle (not used for IN) */
  /* According to datasheet, for IN transactions we set the RX sync toggle in bit 7 */
  sync_flags = info->toggle_in ? 0x80 : 0x00;

  /* Issue IN transaction to interrupt endpoint */
  status = ch376_issue_token_x(spi_id, sync_flags, endpoint, CH376_PID_IN);

  if (status == CH376_INT_SUCCESS)
  {
    /* Toggle for next transfer */
    info->toggle_in = info->toggle_in ? 0 : 1;

    /* Read the data */
    len = ch376_read_data(spi_id, buffer, 64);
    if (len >= 1)
    {
      report->modifier = buffer[0] & 0xFF;
      report->reserved = (len >= 2) ? (buffer[1] & 0xFF) : 0;
      for (i = 0; i < 6; i++)
      {
        report->keycode[i] = (len >= 3 + i) ? (buffer[2 + i] & 0xFF) : 0;
      }
      return 1;
    }
    return 0; /* Got success but no data */
  }
  /* Check for NAK: status byte with bits 5 set and lower bits 1010 = NAK */
  else if ((status & 0x3F) == 0x2A)
  {
    /* NAK - no new data, this is normal for interrupt endpoints */
    return 0;
  }

  // Other error
  return -status;
}

/* HID keycode to ASCII lookup table (US keyboard layout) */
static char hid_keycode_table[] = {
    0, 0, 0, 0,                                       /* 0x00-0x03: Reserved */
    'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h',           /* 0x04-0x0B */
    'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p',           /* 0x0C-0x13 */
    'q', 'r', 's', 't', 'u', 'v', 'w', 'x',           /* 0x14-0x1B */
    'y', 'z',                                         /* 0x1C-0x1D */
    '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', /* 0x1E-0x27 */
    '\n', 27, '\b', '\t',                             /* 0x28-0x2B: Enter, Escape, Backspace, Tab */
    ' ', '-', '=', '[', ']', '\\',                    /* 0x2C-0x31 */
    '#', ';', '\'', '`', ',', '.', '/',               /* 0x32-0x38 */
    0,                                                /* 0x39: Caps Lock */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,               /* 0x3A-0x45: F1-F12 */
    0, 0, 0, 0, 0, 0, 0, 0, 0,                        /* 0x46-0x4E: Various */
    0, 0, 0, 0,                                       /* 0x4F-0x52: Arrows */
    0                                                 /* 0x53: Num Lock */
};

/* Shifted characters */
static char hid_keycode_shift_table[] = {
    0, 0, 0, 0,
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
    'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
    'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
    'Y', 'Z',
    '!', '@', '#', '$', '%', '^', '&', '*', '(', ')',
    '\n', 27, '\b', '\t',
    ' ', '_', '+', '{', '}', '|',
    '~', ':', '"', '~', '<', '>', '?',
    0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0,
    0};

char ch376_keycode_to_ascii(int keycode, int modifier)
{
  int shifted;

  if (keycode >= 84)
  {
    return 0;
  }

  shifted = (modifier & (USB_HID_MOD_LSHIFT | USB_HID_MOD_RSHIFT)) ? 1 : 0;

  if (shifted)
  {
    return hid_keycode_shift_table[keycode];
  }
  return hid_keycode_table[keycode];
}
