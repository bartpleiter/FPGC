#ifndef BDOS_HID_H
#define BDOS_HID_H

// USB keyboard globals
// Global as there is currently no way to pass context to the polling timer callback
int bdos_usb_keyboard_spi_id = CH376_SPI_BOTTOM;
// USB device info struct to store enumeration results for the keyboard
usb_device_info_t bdos_usb_keyboard_device;

// Key state bitmap bit positions
// Updated every HID poll cycle from the raw USB keyboard report.
// Used for real-time "key is held" queries (e.g., smooth scrolling, games).
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

// Current key state bitmap (updated by HID ISR)
unsigned int bdos_key_state_bitmap = 0;

// Special (non-ASCII) keyboard event codes pushed by HID FIFO
#define BDOS_KEY_SPECIAL_BASE 0x100
#define BDOS_KEY_UP           (BDOS_KEY_SPECIAL_BASE + 1)
#define BDOS_KEY_DOWN         (BDOS_KEY_SPECIAL_BASE + 2)
#define BDOS_KEY_LEFT         (BDOS_KEY_SPECIAL_BASE + 3)
#define BDOS_KEY_RIGHT        (BDOS_KEY_SPECIAL_BASE + 4)
#define BDOS_KEY_INSERT       (BDOS_KEY_SPECIAL_BASE + 5)
#define BDOS_KEY_DELETE       (BDOS_KEY_SPECIAL_BASE + 6)
#define BDOS_KEY_HOME         (BDOS_KEY_SPECIAL_BASE + 7)
#define BDOS_KEY_END          (BDOS_KEY_SPECIAL_BASE + 8)
#define BDOS_KEY_PAGEUP       (BDOS_KEY_SPECIAL_BASE + 9)
#define BDOS_KEY_PAGEDOWN     (BDOS_KEY_SPECIAL_BASE + 10)
#define BDOS_KEY_F1           (BDOS_KEY_SPECIAL_BASE + 11)
#define BDOS_KEY_F2           (BDOS_KEY_SPECIAL_BASE + 12)
#define BDOS_KEY_F3           (BDOS_KEY_SPECIAL_BASE + 13)
#define BDOS_KEY_F4           (BDOS_KEY_SPECIAL_BASE + 14)
#define BDOS_KEY_F5           (BDOS_KEY_SPECIAL_BASE + 15)
#define BDOS_KEY_F6           (BDOS_KEY_SPECIAL_BASE + 16)
#define BDOS_KEY_F7           (BDOS_KEY_SPECIAL_BASE + 17)
#define BDOS_KEY_F8           (BDOS_KEY_SPECIAL_BASE + 18)
#define BDOS_KEY_F9           (BDOS_KEY_SPECIAL_BASE + 19)
#define BDOS_KEY_F10          (BDOS_KEY_SPECIAL_BASE + 20)
#define BDOS_KEY_F11          (BDOS_KEY_SPECIAL_BASE + 21)
#define BDOS_KEY_F12          (BDOS_KEY_SPECIAL_BASE + 22)

// Input handling functions
void bdos_poll_usb_keyboard(int timer_id);
int bdos_keyboard_event_available();
int bdos_keyboard_event_read();
int bdos_keyboard_event_fifo_push(int key_event);
void bdos_rebuild_key_state_bitmap(const hid_keyboard_report_t *report);

#endif // BDOS_HID_H
