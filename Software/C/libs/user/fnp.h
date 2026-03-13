#ifndef USER_FNP_H
#define USER_FNP_H

// FNP (FPGC Network Protocol) user-side helpers.
// Builds, parses, and sends FNP frames over raw Ethernet syscalls.

// ---- Protocol constants (must match BDOS bdos_fnp.h) ----

#define FNP_ETHERTYPE       0xB4B4
#define FNP_VERSION         0x01
#define FNP_ETH_HEADER_SIZE 14
#define FNP_HEADER_SIZE     7
#define FNP_MAX_DATA        1024
#define FNP_FRAME_BUF_SIZE  1518

// ---- FNP message types ----

// Core protocol types
#define FNP_TYPE_ACK        0x01
#define FNP_TYPE_NACK       0x02

// File transfer types (for reference; user programs typically don't use these)
#define FNP_TYPE_FILE_START 0x10
#define FNP_TYPE_FILE_DATA  0x11
#define FNP_TYPE_FILE_END   0x12
#define FNP_TYPE_FILE_ABORT 0x13

// Keycode injection
#define FNP_TYPE_KEYCODE    0x20

// General message
#define FNP_TYPE_MESSAGE    0x30

// Cluster demo types
#define FNP_TYPE_CLUSTER_PARAMS  0x40
#define FNP_TYPE_CLUSTER_ASSIGN  0x41
#define FNP_TYPE_CLUSTER_RESULT  0x42

// ---- FNP flags ----
#define FNP_FLAG_MORE_DATA    0x01
#define FNP_FLAG_REQUIRES_ACK 0x02

// ---- FNP error codes ----
#define FNP_ERR_GENERIC 0xFF

// ---- Reliable send parameters ----
#define FNP_ACK_TIMEOUT_MS 100
#define FNP_MAX_RETRIES    3

// ---- API functions ----

// Initialize the FNP user library.  Reads our MAC address via syscall.
// Must be called once before any other fnp_* function.
void fnp_init();

// Build and send an FNP frame.
// dest_mac: 6-element int array with destination MAC bytes
// msg_type: FNP message type
// seq: sequence number
// flags: FNP flags
// data: payload (may be NULL if data_len == 0)
// data_len: payload length in bytes (max FNP_MAX_DATA)
// frame_buf: caller-provided buffer of at least FNP_FRAME_BUF_SIZE bytes
// Returns 1 on success, 0 on failure.
int fnp_send(int *dest_mac, int msg_type, int seq, int flags,
             char *data, int data_len, char *frame_buf);

// Parse a received Ethernet frame as FNP.
// frame_buf: received frame data
// frame_len: received frame length
// src_mac_out: 6-element int array to receive source MAC bytes
// msg_type_out: receives FNP message type
// seq_out: receives sequence number
// flags_out: receives flags byte
// data_out: receives pointer into frame_buf where payload starts
// data_len_out: receives payload length
// Returns 1 if valid FNP frame, 0 otherwise.
int fnp_parse(char *frame_buf, int frame_len,
              int *src_mac_out, int *msg_type_out, int *seq_out,
              int *flags_out, char **data_out, int *data_len_out);

// Send a frame and wait for an ACK (with timeout and retries).
// dest_mac: 6-element int array with destination MAC bytes
// msg_type: FNP message type
// data/data_len: payload
// frame_buf: caller-provided buffer (used for both TX and RX, at least FNP_FRAME_BUF_SIZE)
// seq_counter: pointer to a sequence counter (incremented on each call)
// Returns 1 if ACK received, 0 if all retries exhausted.
int fnp_send_reliable(int *dest_mac, int msg_type,
                      char *data, int data_len,
                      char *frame_buf, int *seq_counter);

// Get our MAC address (cached from fnp_init).
// mac_out: 6-element int array to receive MAC bytes.
void fnp_get_our_mac(int *mac_out);

// Send a single keycode to a remote device (reliable, with ACK).
// dest_mac: 6-element int array with destination MAC bytes
// keycode: ASCII code or HID keycode to inject
// frame_buf: caller-provided buffer of at least FNP_FRAME_BUF_SIZE bytes
// seq_counter: pointer to a sequence counter (incremented on each call)
// Returns 1 on success, 0 on failure.
int fnp_send_keycode(int *dest_mac, int keycode,
                     char *frame_buf, int *seq_counter);

// Send a shell command string followed by Enter to a remote device.
// Each character is sent as a reliable keycode, followed by Enter (0x0A).
// dest_mac: 6-element int array with destination MAC bytes
// cmd: null-terminated command string
// frame_buf: caller-provided buffer of at least FNP_FRAME_BUF_SIZE bytes
// seq_counter: pointer to a sequence counter (incremented on each call)
// Returns 1 if all keycodes sent successfully, 0 on first failure.
int fnp_send_command(int *dest_mac, char *cmd,
                     char *frame_buf, int *seq_counter);

#endif // USER_FNP_H
