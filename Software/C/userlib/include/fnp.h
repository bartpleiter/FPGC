#ifndef FNP_H
#define FNP_H

/*
 * FNP (FPGC Network Protocol) user-side helpers.
 * Builds, parses, and sends FNP frames over raw Ethernet syscalls.
 */

/* Protocol constants (must match BDOS bdos_fnp.h) */
#define FNP_ETHERTYPE       0xB4B4
#define FNP_VERSION         0x01
#define FNP_ETH_HEADER_SIZE 14
#define FNP_HEADER_SIZE     7
#define FNP_MAX_DATA        1024
#define FNP_FRAME_BUF_SIZE  1518

/* FNP message types */
#define FNP_TYPE_ACK        0x01
#define FNP_TYPE_NACK       0x02
#define FNP_TYPE_FILE_START 0x10
#define FNP_TYPE_FILE_DATA  0x11
#define FNP_TYPE_FILE_END   0x12
#define FNP_TYPE_FILE_ABORT 0x13
#define FNP_TYPE_KEYCODE    0x20
#define FNP_TYPE_MESSAGE    0x30
#define FNP_TYPE_CLUSTER_PARAMS  0x40
#define FNP_TYPE_CLUSTER_ASSIGN  0x41
#define FNP_TYPE_CLUSTER_RESULT  0x42
#define FNP_TYPE_TETRIS_PARAMS   0x50
#define FNP_TYPE_TETRIS_ASSIGN   0x51
#define FNP_TYPE_TETRIS_BOARD    0x52
#define FNP_TYPE_TETRIS_RESULT   0x53

/* FNP flags */
#define FNP_FLAG_MORE_DATA    0x01
#define FNP_FLAG_REQUIRES_ACK 0x02

/* FNP error codes */
#define FNP_ERR_GENERIC 0xFF

/* Reliable send parameters */
#define FNP_ACK_TIMEOUT_MS 100
#define FNP_MAX_RETRIES    3

/* Initialize FNP library (reads MAC via syscall). Call once before other fnp_* functions. */
void fnp_init(void);

/* Build and send an FNP frame. Returns 1 on success, 0 on failure. */
int fnp_send(int *dest_mac, int msg_type, int seq, int flags,
             char *data, int data_len, char *frame_buf);

/* Parse a received Ethernet frame as FNP. Returns 1 if valid, 0 otherwise. */
int fnp_parse(char *frame_buf, int frame_len,
              int *src_mac_out, int *msg_type_out, int *seq_out,
              int *flags_out, char **data_out, int *data_len_out);

/* Send a frame and wait for ACK (with timeout/retries). Returns 1 if ACK received. */
int fnp_send_reliable(int *dest_mac, int msg_type,
                      char *data, int data_len,
                      char *frame_buf, int *seq_counter);

/* Get our MAC address (cached from fnp_init). */
void fnp_get_our_mac(int *mac_out);

/* Send a single keycode to a remote device (reliable). Returns 1 on success. */
int fnp_send_keycode(int *dest_mac, int keycode,
                     char *frame_buf, int *seq_counter);

/* Send a shell command string followed by Enter (reliable). Returns 1 on success. */
int fnp_send_command(int *dest_mac, char *cmd,
                     char *frame_buf, int *seq_counter);

#endif /* FNP_H */
