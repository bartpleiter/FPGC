#ifndef BDOS_FNP_H
#define BDOS_FNP_H

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

// FNP state variables

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

// When set, the kernel's bdos_fnp_poll() is disabled so user programs
// can send/receive raw Ethernet frames via syscalls.
// Cleared automatically when the user program exits.
int fnp_net_user_owned = 0;

// ---- Interrupt-driven RX ring buffer ----
// The ENC28J60 ISR drains packets from hardware into this ring buffer.
// Consumers (user programs via NET_RECV, or kernel FNP poll) read from it.
#define NET_RINGBUF_SLOTS      64
#define NET_RINGBUF_FRAME_SIZE 1518

char net_ringbuf_data[NET_RINGBUF_SLOTS * NET_RINGBUF_FRAME_SIZE];
int  net_ringbuf_len[NET_RINGBUF_SLOTS];
int  net_ringbuf_head = 0;  // ISR writes here
int  net_ringbuf_tail = 0;  // consumer reads here

// Set when the ETH ISR was deferred due to SPI mutex.
// TIMER_0 retries the drain when this is set.
int net_isr_deferred = 0;

// Network FNP functions
void bdos_fnp_poll();
void bdos_fnp_init();

// ISR drain: read all pending packets from ENC28J60 into ring buffer
void bdos_net_isr_drain();

// Reset the ring buffer (called on user program exit)
void bdos_net_ringbuf_reset();

#endif // BDOS_FNP_H
