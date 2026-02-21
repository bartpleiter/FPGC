#ifndef BDOS_H
#define BDOS_H

// ---- Imports ----

// Include common libraries
#define COMMON_STRING
#define COMMON_STDLIB
#define COMMON_CTYPE
#include "libs/common/common.h"

// Include kernel libraries
#define KERNEL_GPU_HAL
#define KERNEL_GPU_FB
#define KERNEL_GPU_DATA_ASCII
#define KERNEL_TERM
#define KERNEL_UART
#define KERNEL_SPI
#define KERNEL_SPI_FLASH
#define KERNEL_BRFS
#define KERNEL_TIMER
#define KERNEL_CH376
#define KERNEL_ENC28J60
#include "libs/kernel/kernel.h"

// Include memory map definitions
#include "BDOS/mem_map.h"

// ---- Global variables and defines ----

// USB keyboard variables
// Global as there is currently no way to pass context to the polling timer callback
int bdos_usb_keyboard_spi_id = CH376_SPI_BOTTOM;
// USB device info struct to store enumeration results for the keyboard
usb_device_info_t bdos_usb_keyboard_device;

// ---- FNP Protocol Constants ----

// FNP EtherType
#define FNP_ETHERTYPE 0xB4B4

// FNP protocol version
#define FNP_VERSION 0x01

// FNP header size (Ver + Type + Seq + Flags + Length)
#define FNP_HEADER_SIZE 7

// Ethernet header size (Dst MAC + Src MAC + EtherType)
#define FNP_ETH_HEADER_SIZE 14

// Maximum raw frame buffer (max Ethernet frame without CRC)
#define FNP_FRAME_BUF_SIZE 1518

// Maximum FNP data payload per frame
#define FNP_MAX_DATA 1024

// Maximum FILE_DATA chunk in bytes (must be multiple of 4)
#define FNP_FILE_CHUNK_SIZE 1024

// FNP message types
#define FNP_TYPE_ACK        0x01
#define FNP_TYPE_NACK       0x02
#define FNP_TYPE_FILE_START 0x10
#define FNP_TYPE_FILE_DATA  0x11
#define FNP_TYPE_FILE_END   0x12
#define FNP_TYPE_FILE_ABORT 0x13
#define FNP_TYPE_KEYCODE    0x20
#define FNP_TYPE_MESSAGE    0x30

// FNP flags
#define FNP_FLAG_MORE_DATA    0x01
#define FNP_FLAG_REQUIRES_ACK 0x02

// FNP error codes
#define FNP_ERR_GENERIC 0xFF

// ACK timeout and retry
#define FNP_ACK_TIMEOUT_US 100000
#define FNP_MAX_RETRIES    2

// FNP file transfer states
#define FNP_STATE_IDLE      0
#define FNP_STATE_RECEIVING 1

// ---- FNP State Variables ----

// RX frame buffer (single buffer, processed inline)
char fnp_rx_buf[FNP_FRAME_BUF_SIZE];

// TX frame buffer
char fnp_tx_buf[FNP_FRAME_BUF_SIZE];

// Our MAC address (set during init)
int fnp_our_mac[6];

// Peer MAC address (saved from last received frame for replies)
char fnp_peer_mac[6];

// Per-peer TX sequence counter
int fnp_tx_seq = 0;

// File transfer state
int fnp_transfer_state = 0;
int fnp_transfer_fd = -1;
unsigned int fnp_transfer_checksum = 0;
unsigned int fnp_transfer_size = 0;
unsigned int fnp_transfer_received = 0;

// HID configuration
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

// Shell configuration and variables
#define BDOS_SHELL_INPUT_MAX   160
#define BDOS_SHELL_ARGV_MAX    8
#define BDOS_SHELL_PROMPT_MAX  192
#define BDOS_SHELL_PATH_MAX    (BRFS_MAX_PATH_LENGTH + 1)

extern char bdos_shell_cwd[BDOS_SHELL_PATH_MAX];
extern unsigned int bdos_shell_start_micros;

// BRFS flash target
#define BDOS_FS_FLASH_ID SPI_FLASH_1

// Shared filesystem state
extern int bdos_fs_ready;
extern int bdos_fs_boot_needs_format;
extern int bdos_fs_last_mount_error;

// ---- Function declarations ----

void bdos_panic(char* msg);

void bdos_init();

void bdos_poll_usb_keyboard(int timer_id);

int bdos_keyboard_event_available();

int bdos_keyboard_event_read();

int bdos_keyboard_event_fifo_push(int key_event);

void bdos_fs_boot_init();

int bdos_fs_format_and_sync(unsigned int total_blocks, unsigned int words_per_block,
							char* label, int full_format);

int bdos_fs_sync_now();

char* bdos_fs_error_string(int error_code);

void bdos_shell_init();

void bdos_shell_tick();

void bdos_shell_execute_line(char* line);

int bdos_shell_handle_special_mode_line(char* line);

void bdos_shell_on_startup();

void bdos_fnp_poll();
void bdos_fnp_init();

#endif // BDOS_H
