/*
 * hid.h — USB keyboard subsystem.
 *
 * Handles CH376 USB host connect/disconnect detection (main loop)
 * and HID report reading (Timer 1 ISR at 10ms intervals).
 * Produces key events into a FIFO consumed by /dev/tty.
 */
#ifndef KERNEL_HID_H
#define KERNEL_HID_H

/* Key event FIFO */
#define KEY_FIFO_SIZE 64

/* Key state bitmap bit positions */
#define KEYSTATE_W        0x0001
#define KEYSTATE_A        0x0002
#define KEYSTATE_S        0x0004
#define KEYSTATE_D        0x0008
#define KEYSTATE_UP       0x0010
#define KEYSTATE_DOWN     0x0020
#define KEYSTATE_LEFT     0x0040
#define KEYSTATE_RIGHT    0x0080
#define KEYSTATE_SPACE    0x0100
#define KEYSTATE_SHIFT    0x0200
#define KEYSTATE_CTRL     0x0400
#define KEYSTATE_ESCAPE   0x0800
#define KEYSTATE_E        0x1000
#define KEYSTATE_Q        0x2000

/* Special (non-ASCII) keyboard event codes */
#define KEY_SPECIAL_BASE 0x100
#define KEY_UP           (KEY_SPECIAL_BASE + 1)
#define KEY_DOWN         (KEY_SPECIAL_BASE + 2)
#define KEY_LEFT         (KEY_SPECIAL_BASE + 3)
#define KEY_RIGHT        (KEY_SPECIAL_BASE + 4)
#define KEY_INSERT       (KEY_SPECIAL_BASE + 5)
#define KEY_DELETE       (KEY_SPECIAL_BASE + 6)
#define KEY_HOME         (KEY_SPECIAL_BASE + 7)
#define KEY_END          (KEY_SPECIAL_BASE + 8)
#define KEY_PAGEUP       (KEY_SPECIAL_BASE + 9)
#define KEY_PAGEDOWN     (KEY_SPECIAL_BASE + 10)
#define KEY_F1           (KEY_SPECIAL_BASE + 11)
#define KEY_F2           (KEY_SPECIAL_BASE + 12)
#define KEY_F3           (KEY_SPECIAL_BASE + 13)
#define KEY_F4           (KEY_SPECIAL_BASE + 14)
#define KEY_F5           (KEY_SPECIAL_BASE + 15)
#define KEY_F6           (KEY_SPECIAL_BASE + 16)
#define KEY_F7           (KEY_SPECIAL_BASE + 17)
#define KEY_F8           (KEY_SPECIAL_BASE + 18)
#define KEY_F9           (KEY_SPECIAL_BASE + 19)
#define KEY_F10          (KEY_SPECIAL_BASE + 20)
#define KEY_F11          (KEY_SPECIAL_BASE + 21)
#define KEY_F12          (KEY_SPECIAL_BASE + 22)

/* Initialize USB keyboard subsystem. */
void hid_init(void);

/* Main-loop polling: check for USB connect/disconnect. */
void hid_poll(void);

/* Timer ISR callback: read HID report if keyboard connected. */
void hid_timer_callback(int timer_id);

/* Read next key event from FIFO. Returns the key code, or -1 if empty. */
int hid_event_read(void);

/* Check if events are available. */
int hid_event_available(void);

/* Push a key event into the FIFO (used by FNP remote keyboard). */
int hid_event_push(int key);

/* Real-time held-key bitmap (rebuilt from HID report each poll). */
extern unsigned int hid_key_state;
extern int ctrl_c_pending;

/* USB device state */
extern usb_device_info_t hid_usb_device;
extern int hid_keyboard_connected;
extern int hid_spi_id;

#endif /* KERNEL_HID_H */
