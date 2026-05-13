/*
 * hid.c — USB keyboard HID subsystem.
 *
 * Phase 1 port from v3: same CH376 polling logic, adapted
 * to v4 kernel API. Key events pushed to FIFO consumed by /dev/tty.
 *
 * Uses CH376 on SPI_USB_1 (bottom port). Timer 1 ISR polls HID
 * reports every 10ms. Supports key repeat and modifier keys.
 */
#include "kernel.h"

/* USB device state */
usb_device_info_t hid_usb_device;
int hid_keyboard_connected;
int hid_spi_id;
unsigned int hid_key_state;

/* Key event FIFO */
static int key_fifo[KEY_FIFO_SIZE];
static int key_fifo_head;
static int key_fifo_tail;

/* Previous HID report for key-up/key-down detection */
static hid_keyboard_report_t hid_prev_report;

/* Key repeat state */
#define KEY_REPEAT_DELAY_US    400000U
#define KEY_REPEAT_INTERVAL_US  80000U
static int repeat_keycode;
static int repeat_event;
static int repeat_modifier;
static unsigned int repeat_start_us;
static unsigned int repeat_last_us;

static int key_fifo_count(void)
{
    int h;
    int t;
    h = key_fifo_head;
    t = key_fifo_tail;
    if (h >= t)
        return h - t;
    return KEY_FIFO_SIZE - t + h;
}

int hid_event_available(void)
{
    return key_fifo_count();
}

int hid_event_read(void)
{
    int ch;
    if (key_fifo_head == key_fifo_tail) return -1;
    ch = key_fifo[key_fifo_tail];
    key_fifo_tail = (key_fifo_tail + 1) % KEY_FIFO_SIZE;
    return ch;
}

static int hid_event_push(int key)
{
    int next;
    next = (key_fifo_head + 1) % KEY_FIFO_SIZE;
    if (next == key_fifo_tail) return 0; /* full */
    key_fifo[key_fifo_head] = key;
    key_fifo_head = next;
    return 1;
}

/* Translate a HID keycode + modifier to a key event (ASCII or special) */
static int hid_translate_key(int keycode, int modifier)
{
    int ascii;
    int ctrl_held;

    ctrl_held = modifier & (USB_HID_MOD_LCTRL | USB_HID_MOD_RCTRL);

    /* Ctrl+letter → control character */
    if (ctrl_held && keycode >= 0x04 && keycode <= 0x1D)
        return keycode - 0x03;

    /* Use CH376 library's built-in keycode→ASCII conversion */
    ascii = ch376_keycode_to_ascii(keycode, modifier);
    if (ascii)
        return ascii & 0xFF;

    /* Special keys */
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
    default:   return 0;
    }
}

static int hid_keycode_in_report(const hid_keyboard_report_t *report, int keycode)
{
    int i;
    for (i = 0; i < 6; i++)
    {
        if (report->keycode[i] == keycode)
            return 1;
    }
    return 0;
}

static int hid_find_new_keycode(const hid_keyboard_report_t *prev,
                                const hid_keyboard_report_t *curr)
{
    int i;
    int kc;
    for (i = 0; i < 6; i++)
    {
        kc = curr->keycode[i];
        if (kc == 0) continue;
        if (!hid_keycode_in_report(prev, kc))
            return kc;
    }
    return 0;
}

/* Rebuild key state bitmap from a HID report */
static void hid_rebuild_key_state(const hid_keyboard_report_t *report)
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
        if (kc == 0) continue;
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

    hid_key_state = bitmap;
}

static void hid_clear_repeat(void)
{
    repeat_keycode = 0;
    repeat_event = 0;
    repeat_modifier = 0;
    repeat_start_us = 0;
    repeat_last_us = 0;
}

static void hid_reset_state(void)
{
    memset(&hid_prev_report, 0, sizeof(hid_keyboard_report_t));
    memset(&hid_usb_device, 0, sizeof(usb_device_info_t));
    key_fifo_head = 0;
    key_fifo_tail = 0;
    hid_clear_repeat();
    hid_key_state = 0;
}

/* Timer ISR callback: read HID report from CH376. Called from interrupt(). */
void hid_timer_callback(int timer_id)
{
    (void)timer_id;
    hid_keyboard_report_t kb_report;
    hid_keyboard_report_t *active_report;
    int read_status;
    int new_keycode;
    int key_event;
    unsigned int now;

    if (!hid_usb_device.connected) return;
    if (!ch376_is_keyboard(&hid_usb_device)) return;

    now = get_micros();
    read_status = ch376_read_keyboard(hid_spi_id, &hid_usb_device, &kb_report);

    if (read_status == 1)
    {
        new_keycode = hid_find_new_keycode(&hid_prev_report, &kb_report);
        hid_rebuild_key_state(&kb_report);

        if (new_keycode)
        {
            key_event = hid_translate_key(new_keycode, kb_report.modifier);
            if (key_event)
            {
                hid_event_push(key_event);
            }
            repeat_keycode = new_keycode;
            repeat_modifier = kb_report.modifier;
            repeat_event = key_event;
            repeat_start_us = now;
            repeat_last_us = now;
        }

        memcpy(&hid_prev_report, &kb_report, sizeof(hid_keyboard_report_t));
    }
    else
    {
        hid_rebuild_key_state(&hid_prev_report);
    }

    /* Key repeat logic */
    if (repeat_keycode)
    {
        active_report = (read_status == 1) ? &kb_report : &hid_prev_report;

        if (!hid_keycode_in_report(active_report, repeat_keycode))
        {
            hid_clear_repeat();
        }
        else
        {
            if (active_report->modifier != repeat_modifier)
            {
                key_event = hid_translate_key(repeat_keycode, active_report->modifier);
                repeat_modifier = active_report->modifier;
                repeat_event = key_event;
                repeat_start_us = now;
                repeat_last_us = now;
            }

            if (repeat_event &&
                (unsigned int)(now - repeat_start_us) >= KEY_REPEAT_DELAY_US &&
                (unsigned int)(now - repeat_last_us) >= KEY_REPEAT_INTERVAL_US)
            {
                if (hid_event_push(repeat_event))
                    repeat_last_us = now;
            }
        }
    }
}

/* Main-loop polling: check for USB connect/disconnect via INT# pin. */
void hid_poll(void)
{
    int status;

    /* Only act when CH376 has asserted INT# (active low) */
    if (!ch376_read_int(hid_spi_id))
        return;

    /* Read and clear the interrupt status */
    status = ch376_get_status(hid_spi_id);

    if (status == CH376_INT_DISCONNECT)
    {
        if (hid_usb_device.connected)
        {
            hid_reset_state();
            kernel_log("USB keyboard disconnected\n");
        }
    }
    else if (status == CH376_INT_CONNECT)
    {
        delay(50);
        if (ch376_enumerate_device(hid_spi_id, &hid_usb_device))
        {
            kernel_log("USB keyboard connected\n");
        }
        else
        {
            kernel_log("USB enumerate failed\n");
        }
    }
}

void hid_init(void)
{
    hid_spi_id = CH376_SPI_BOTTOM;
    hid_keyboard_connected = 0;
    hid_key_state = 0;
    key_fifo_head = 0;
    key_fifo_tail = 0;
    hid_reset_state();

    ch376_host_init(hid_spi_id);

    /* Set up Timer 1 for 10ms HID polling */
    timer_set_callback(TIMER_1, hid_timer_callback);
    timer_start_periodic(TIMER_1, 10);
}
