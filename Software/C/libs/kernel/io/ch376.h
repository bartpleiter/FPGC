#ifndef CH376_H
#define CH376_H

// Library for the CH376 USB interface chip.
// Provides generic USB host functionality (like CH375/Arduino USB Host Shield).
// Does not support device mode nor the FAT file system internals.
// Builds on top of the SPI library.
// Supports:
// - Device detection and connection status
// - Device descriptor reading
// - HID keyboard input
// - Extensible for other USB device types

// SPI IDs for the two CH376 chips
#define CH376_SPI_TOP       SPI_ID_USB_0
#define CH376_SPI_BOTTOM    SPI_ID_USB_1

// CH376 Command codes
#define CH376_CMD_GET_IC_VER        0x01
#define CH376_CMD_ENTER_SLEEP       0x03
#define CH376_CMD_SET_USB_SPEED     0x04
#define CH376_CMD_RESET_ALL         0x05
#define CH376_CMD_CHECK_EXIST       0x06
#define CH376_CMD_GET_DEV_RATE      0x0A
#define CH376_CMD_SET_RETRY         0x0B
#define CH376_CMD_SET_USB_MODE      0x15
#define CH376_CMD_TEST_CONNECT      0x16
#define CH376_CMD_ABORT_NAK         0x17
#define CH376_CMD_SET_ENDP6         0x1C
#define CH376_CMD_SET_ENDP7         0x1D
#define CH376_CMD_GET_STATUS        0x22
#define CH376_CMD_RD_USB_DATA0      0x27
#define CH376_CMD_RD_USB_DATA       0x28
#define CH376_CMD_WR_HOST_DATA      0x2C
#define CH376_CMD_SET_USB_ADDR      0x13
#define CH376_CMD_SET_ADDRESS       0x45
#define CH376_CMD_GET_DESCR         0x46
#define CH376_CMD_SET_CONFIG        0x49
#define CH376_CMD_AUTO_SETUP        0x4D
#define CH376_CMD_ISSUE_TKN_X       0x4E
#define CH376_CMD_ISSUE_TOKEN       0x4F
#define CH376_CMD_CLR_STALL         0x41

// USB Mode codes
#define CH376_MODE_DISABLED         0x00
#define CH376_MODE_HOST_DISABLED    0x04
#define CH376_MODE_HOST_ENABLED     0x05
#define CH376_MODE_HOST_SOF         0x06
#define CH376_MODE_HOST_RESET       0x07

// Interrupt status codes
#define CH376_INT_SUCCESS           0x14
#define CH376_INT_CONNECT           0x15
#define CH376_INT_DISCONNECT        0x16
#define CH376_INT_BUF_OVER          0x17
#define CH376_INT_USB_READY         0x18

// USB Speed settings
#define CH376_SPEED_FULL            0x00 // 12 Mbps
#define CH376_SPEED_FULL_LOW        0x01 // 1.5 Mbps (non-standard)
#define CH376_SPEED_LOW             0x02 // 1.5 Mbps low speed

// USB PIDs for transactions
#define CH376_PID_SETUP             0x0D
#define CH376_PID_OUT               0x01
#define CH376_PID_IN                0x09

// Descriptor types
#define CH376_DESCR_DEVICE          0x01
#define CH376_DESCR_CONFIG          0x02
#define CH376_DESCR_STRING          0x03
#define CH376_DESCR_INTERFACE       0x04
#define CH376_DESCR_ENDPOINT        0x05
#define CH376_DESCR_HID             0x21
#define CH376_DESCR_HID_REPORT      0x22

// USB Device Classes
#define USB_CLASS_HID               0x03
#define USB_CLASS_MASS_STORAGE      0x08
#define USB_CLASS_HUB               0x09

// HID Subclass and Protocol
#define USB_HID_SUBCLASS_BOOT       0x01
#define USB_HID_PROTOCOL_KEYBOARD   0x01
#define USB_HID_PROTOCOL_MOUSE      0x02

// HID Keyboard modifier bits
#define USB_HID_MOD_LCTRL           0x01
#define USB_HID_MOD_LSHIFT          0x02
#define USB_HID_MOD_LALT            0x04
#define USB_HID_MOD_LGUI            0x08
#define USB_HID_MOD_RCTRL           0x10
#define USB_HID_MOD_RSHIFT          0x20
#define USB_HID_MOD_RALT            0x40
#define USB_HID_MOD_RGUI            0x80

// Connection status
#define CH376_CONN_DISCONNECTED     0
#define CH376_CONN_CONNECTED        1
#define CH376_CONN_READY            2
#define CH376_CONN_UNKNOWN          3

// USB Device Descriptor structure (18 bytes)
typedef struct {
    int bLength;
    int bDescriptorType;
    int bcdUSB;
    int bDeviceClass;
    int bDeviceSubClass;
    int bDeviceProtocol;
    int bMaxPacketSize0;
    int idVendor;
    int idProduct;
    int bcdDevice;
    int iManufacturer;
    int iProduct;
    int iSerialNumber;
    int bNumConfigurations;
} usb_device_descriptor_t;

// USB Configuration Descriptor structure (9 bytes)
typedef struct {
    int bLength;
    int bDescriptorType;
    int wTotalLength;
    int bNumInterfaces;
    int bConfigurationValue;
    int iConfiguration;
    int bmAttributes;
    int bMaxPower;
} usb_config_descriptor_t;

// USB Interface Descriptor structure (9 bytes)
typedef struct {
    int bLength;
    int bDescriptorType;
    int bInterfaceNumber;
    int bAlternateSetting;
    int bNumEndpoints;
    int bInterfaceClass;
    int bInterfaceSubClass;
    int bInterfaceProtocol;
    int iInterface;
} usb_interface_descriptor_t;

// USB Endpoint Descriptor structure (7 bytes)
typedef struct {
    int bLength;
    int bDescriptorType;
    int bEndpointAddress;
    int bmAttributes;
    int wMaxPacketSize;
    int bInterval;
} usb_endpoint_descriptor_t;

// HID Keyboard report structure (8 bytes)
typedef struct {
    int modifier; // Modifier keys (Ctrl, Shift, Alt, GUI)
    int reserved; // Reserved (always 0)
    int keycode[6]; // Up to 6 simultaneous key presses
} hid_keyboard_report_t;

// USB Device info structure
typedef struct {
    int connected; // Connection status
    int low_speed; // 1 if low speed device
    int address; // Assigned USB address
    usb_device_descriptor_t device_desc;
    int interface_class; // Class of first interface
    int interface_subclass; // Subclass of first interface
    int interface_protocol; // Protocol of first interface
    int interrupt_endpoint; // Interrupt IN endpoint address (for HID)
    int interrupt_max_packet; // Max packet size for interrupt endpoint
    int toggle_in; // Data toggle for IN endpoint
} usb_device_info_t;

// ============================================
// Basic CH376 Functions
// ============================================

// Read the interrupt pin state (active low, inverted in function).
int ch376_read_int(int spi_id);

// Get chip version.
int ch376_get_version(int spi_id);

// Check if CH376 is working properly.
int ch376_check_exist(int spi_id);

// Reset the CH376 chip.
void ch376_reset(int spi_id);

// Set USB working mode.
int ch376_set_usb_mode(int spi_id, int mode);

// Set USB bus speed.
void ch376_set_usb_speed(int spi_id, int speed);

// Get interrupt status and clear interrupt.
int ch376_get_status(int spi_id);

// Wait for interrupt with timeout.
int ch376_wait_interrupt(int spi_id, int timeout_ms);

// Test USB device connection status.
int ch376_test_connect(int spi_id);

// Set retry behavior for USB transactions.
void ch376_set_retry(int spi_id, int retry);

// Read data from USB buffer.
int ch376_read_data(int spi_id, char* buffer, int max_len);

// Write data to USB host buffer.
void ch376_write_data(int spi_id, char* buffer, int len);

// ============================================
// USB Host Functions
// ============================================

// Initialize CH376 as USB host.
int ch376_host_init(int spi_id);

// Check for device connection and get rate type.
int ch376_detect_device(int spi_id, int* is_low_speed);

// Get device descriptor using simplified command.
int ch376_get_device_descriptor(int spi_id, usb_device_descriptor_t* desc);

// Get configuration descriptor (first 64 bytes max).
int ch376_get_config_descriptor(int spi_id, char* buffer, int max_len);

// Get configuration descriptor using manual control transfer.
// Use this if the simplified command doesn't work.
int ch376_get_config_descriptor_manual(int spi_id, char* buffer, int max_len);

// Set USB device address.
int ch376_set_device_address(int spi_id, int address);

// Set USB device configuration.
int ch376_set_device_config(int spi_id, int config);

// Execute a USB transaction.
int ch376_issue_token(int spi_id, int endpoint, int pid);

// Execute a USB transaction with explicit sync flags (CMD_ISSUE_TKN_X).
int ch376_issue_token_x(int spi_id, int sync_flags, int endpoint, int pid);

// Set receiver (host IN endpoint) toggle.
void ch376_set_rx_toggle(int spi_id, int toggle);

// Set transmitter (host OUT endpoint) toggle.
void ch376_set_tx_toggle(int spi_id, int toggle);

// ============================================
// High-Level USB Functions
// ============================================

// Initialize and enumerate a USB device.
int ch376_enumerate_device(int spi_id, usb_device_info_t* info);

// Check if device is a HID keyboard.
int ch376_is_keyboard(usb_device_info_t* info);

// Check if device is a HID mouse.
int ch376_is_mouse(usb_device_info_t* info);

// Read HID keyboard report (poll for key presses).
int ch376_read_keyboard(int spi_id, usb_device_info_t* info, hid_keyboard_report_t* report);

// Convert HID keycode to ASCII character.
char ch376_keycode_to_ascii(int keycode, int modifier);

// Set HID idle rate (for keyboard).
int ch376_hid_set_idle(int spi_id, usb_device_info_t* info, int duration);

// Set HID protocol (boot or report).
int ch376_hid_set_protocol(int spi_id, usb_device_info_t* info, int protocol);

#endif // CH376_H
