#ifndef BDOS_FNP_H
#define BDOS_FNP_H

/* FNP EtherType */
#define FNP_ETHERTYPE 0xB4B4

/* FNP protocol version */
#define FNP_VERSION 0x01

/* FNP header size (Ver + Type + Seq + Flags + Length) */
#define FNP_HEADER_SIZE 7

/* Ethernet header size (Dst MAC + Src MAC + EtherType) */
#define FNP_ETH_HEADER_SIZE 14

/* Maximum raw frame buffer (max Ethernet frame without CRC) */
#define FNP_FRAME_BUF_SIZE 1518

/* Maximum FNP data payload per frame */
#define FNP_MAX_DATA 1024

/* Maximum FILE_DATA chunk in bytes (must be multiple of 4) */
#define FNP_FILE_CHUNK_SIZE 1024

/* FNP message types */
#define FNP_TYPE_ACK        0x01
#define FNP_TYPE_NACK       0x02
#define FNP_TYPE_FILE_START 0x10
#define FNP_TYPE_FILE_DATA  0x11
#define FNP_TYPE_FILE_END   0x12
#define FNP_TYPE_FILE_ABORT 0x13
#define FNP_TYPE_KEYCODE    0x20
#define FNP_TYPE_MESSAGE    0x30

/* FNP flags */
#define FNP_FLAG_MORE_DATA    0x01
#define FNP_FLAG_REQUIRES_ACK 0x02

/* FNP error codes */
#define FNP_ERR_GENERIC 0xFF

/* ACK timeout and retry */
#define FNP_ACK_TIMEOUT_US 100000
#define FNP_MAX_RETRIES    2

/* FNP file transfer states */
#define FNP_STATE_IDLE      0
#define FNP_STATE_RECEIVING 1

/* Ring buffer configuration */
#define NET_RINGBUF_SLOTS      32
#define NET_RINGBUF_FRAME_SIZE 1518

/* Extern state variables */
extern char fnp_rx_buf[];
extern char fnp_tx_buf[];
extern int fnp_our_mac[];
extern char fnp_peer_mac[];
extern int fnp_tx_seq;
extern int fnp_transfer_state;
extern int fnp_transfer_fd;
extern unsigned int fnp_transfer_checksum;
extern unsigned int fnp_transfer_size;
extern unsigned int fnp_transfer_received;
extern int fnp_net_user_owned;

extern char net_ringbuf_data[];
extern int net_ringbuf_len[];
extern int net_ringbuf_head;
extern int net_ringbuf_tail;
extern int net_isr_deferred;

/* Network FNP functions */
void bdos_fnp_init(void);
void bdos_fnp_poll(void);

/* ISR drain: read all pending packets from ENC28J60 into ring buffer */
void bdos_net_isr_drain(void);

/* Reset the ring buffer (called on user program exit) */
void bdos_net_ringbuf_reset(void);

/* Ring buffer query (used by syscall) */
int bdos_net_ringbuf_pop(char *buf, int max_len);
int bdos_net_ringbuf_count(void);

#endif /* BDOS_FNP_H */
