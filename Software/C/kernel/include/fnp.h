/*
 * fnp.h — FNP (FPGC Network Protocol) file transfer handler.
 *
 * Phase 1: runs in-kernel, called from the shell idle loop.
 * Phase 2: extract to a standalone daemon (/bin/fnpd on SD card).
 *
 * Protocol: raw Ethernet (EtherType 0xB4B4), supports file upload
 * (FILE_START/DATA/END), remote keycodes, and text messages.
 */
#ifndef KERNEL_FNP_H
#define KERNEL_FNP_H

/* FNP message types */
#define FNP_TYPE_ACK         0x01
#define FNP_TYPE_NACK        0x02
#define FNP_TYPE_FILE_START  0x10
#define FNP_TYPE_FILE_DATA   0x11
#define FNP_TYPE_FILE_END    0x12
#define FNP_TYPE_FILE_ABORT  0x13
#define FNP_TYPE_KEYCODE     0x20
#define FNP_TYPE_MKDIR       0x21
#define FNP_TYPE_SYNC        0x22
#define FNP_TYPE_MESSAGE     0x30

/* FNP flags */
#define FNP_FLAG_MORE_DATA    0x01
#define FNP_FLAG_REQUIRES_ACK 0x02

/* FNP header offsets (after 14-byte Ethernet header) */
#define FNP_HDR_VERSION  14   /* 1 byte */
#define FNP_HDR_TYPE     15   /* 1 byte */
#define FNP_HDR_SEQ      16   /* 2 bytes, big-endian */
#define FNP_HDR_FLAGS    18   /* 1 byte */
#define FNP_HDR_LENGTH   19   /* 2 bytes, big-endian */
#define FNP_HDR_DATA     21   /* payload starts here */

/* Error codes */
#define FNP_ERR_GENERIC  0xFF

/* Buffer sizes */
#define FNP_FRAME_MAX    1518

/* Initialize FNP handler. */
void fnp_init(void);

/*
 * Poll for and handle one FNP packet from the ring buffer.
 * Returns 1 if a packet was processed, 0 if ring was empty.
 *
 * Phase 1: called from shell idle loop.
 * Phase 2: will be the main loop of /bin/fnpd daemon.
 */
int fnp_poll(void);

#endif /* KERNEL_FNP_H */
