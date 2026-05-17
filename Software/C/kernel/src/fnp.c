/*
 * fnp.c — FNP (FPGC Network Protocol) file transfer handler.
 *
 * Phase 1: runs in-kernel, called from the shell idle loop.
 * Phase 2 TODO: extract this into a standalone daemon (/bin/fnpd)
 * that runs as a background process. The daemon should be stored
 * on the SD card so it survives SPI flash reformats.
 *
 * Protocol: raw Ethernet frames with EtherType 0xB4B4.
 * Supports FILE_START/DATA/END for uploading files, KEYCODE for
 * remote keyboard input, and MESSAGE for text display.
 */
#include "kernel.h"

/* ---- Local state ---- */

static char fnp_rx[FNP_FRAME_MAX];
static char fnp_tx[FNP_FRAME_MAX];
static char fnp_peer_mac[6];    /* MAC of connected peer */
static int  fnp_has_peer;       /* Have we seen a peer yet? */

/* File transfer state */
#define FNP_STATE_IDLE      0
#define FNP_STATE_RECEIVING 1

static int          fnp_state;
static int          fnp_transfer_gfd;   /* Global fd for VFS write */
static unsigned int fnp_transfer_checksum;
static unsigned int fnp_transfer_size;     /* Expected size in words */
static unsigned int fnp_transfer_received; /* Words received so far */

/* ---- Helpers ---- */

static unsigned int fnp_read_u16(const char *buf, int offset)
{
    return ((unsigned int)(unsigned char)buf[offset] << 8)
         | ((unsigned int)(unsigned char)buf[offset + 1]);
}

static unsigned int fnp_read_u32(const char *buf, int offset)
{
    return ((unsigned int)(unsigned char)buf[offset]     << 24)
         | ((unsigned int)(unsigned char)buf[offset + 1] << 16)
         | ((unsigned int)(unsigned char)buf[offset + 2] <<  8)
         | ((unsigned int)(unsigned char)buf[offset + 3]);
}

static void fnp_write_u16(char *buf, int offset, unsigned int val)
{
    buf[offset]     = (char)((val >> 8) & 0xFF);
    buf[offset + 1] = (char)(val & 0xFF);
}

/* Build and send an FNP response frame */
static void fnp_send(int type, int seq, const char *data, int data_len)
{
    int i;
    int frame_len;

    /* Ethernet header: dst MAC, src MAC, EtherType */
    for (i = 0; i < 6; i++)
        fnp_tx[i] = fnp_peer_mac[i];
    for (i = 0; i < 6; i++)
        fnp_tx[6 + i] = (char)net_mac[i];
    fnp_tx[12] = (char)0xB4;
    fnp_tx[13] = (char)0xB4;

    /* FNP header */
    fnp_tx[FNP_HDR_VERSION] = 1;
    fnp_tx[FNP_HDR_TYPE] = (char)type;
    fnp_write_u16(fnp_tx, FNP_HDR_SEQ, (unsigned int)seq);
    fnp_tx[FNP_HDR_FLAGS] = 0;
    fnp_write_u16(fnp_tx, FNP_HDR_LENGTH, (unsigned int)data_len);

    /* Payload */
    for (i = 0; i < data_len && i < FNP_FRAME_MAX - FNP_HDR_DATA; i++)
        fnp_tx[FNP_HDR_DATA + i] = data[i];

    frame_len = FNP_HDR_DATA + data_len;
    if (frame_len < 60) frame_len = 60; /* Ethernet minimum */

    enc28j60_packet_send(fnp_tx, frame_len);
}

static void fnp_send_ack(int seq)
{
    char payload[2];
    fnp_write_u16(payload, 0, (unsigned int)seq);
    fnp_send(FNP_TYPE_ACK, seq, payload, 2);
}

static void fnp_send_nack(int seq, const char *msg)
{
    char payload[64];
    int i;
    int len;

    fnp_write_u16(payload, 0, (unsigned int)seq);
    payload[2] = (char)FNP_ERR_GENERIC;

    len = 3;
    for (i = 0; msg[i] && len < 63; i++)
        payload[len++] = msg[i];
    payload[len++] = '\0';

    fnp_send(FNP_TYPE_NACK, seq, payload, len);
}

static void fnp_abort_transfer(void)
{
    if (fnp_transfer_gfd >= 0)
    {
        vfs_close(fnp_transfer_gfd);
        fnp_transfer_gfd = -1;
    }
    fnp_state = FNP_STATE_IDLE;
    fnp_transfer_checksum = 0;
    fnp_transfer_size = 0;
    fnp_transfer_received = 0;
}

/* ---- Message handlers ---- */

static void fnp_handle_file_start(int seq, const char *data, int data_len)
{
    unsigned int path_len;
    unsigned int file_size_words;
    char path_buf[128];
    int i;

    if (fnp_state != FNP_STATE_IDLE)
    {
        fnp_abort_transfer();
    }

    if (data_len < 6)
    {
        fnp_send_nack(seq, "short FILE_START");
        return;
    }

    path_len = fnp_read_u16(data, 0);
    file_size_words = fnp_read_u32(data, 2);

    if (path_len == 0 || path_len > 127 || (int)path_len + 6 > data_len)
    {
        fnp_send_nack(seq, "bad path length");
        return;
    }

    for (i = 0; i < (int)path_len && i < 127; i++)
        path_buf[i] = data[6 + i];
    path_buf[i] = '\0';

    /* Delete existing file first, then create fresh */
    vfs_unlink(path_buf);

    fnp_transfer_gfd = vfs_open(path_buf, 0x02 | 0x08); /* O_WRONLY | O_CREAT */
    if (fnp_transfer_gfd < 0)
    {
        fnp_send_nack(seq, "cannot create file");
        return;
    }

    fnp_state = FNP_STATE_RECEIVING;
    fnp_transfer_checksum = 0;
    fnp_transfer_size = file_size_words;
    fnp_transfer_received = 0;

    term_puts("[FNP] recv: ");
    term_puts(path_buf);
    term_putchar('\n');

    fnp_send_ack(seq);
}

static void fnp_handle_file_data(int seq, const char *data, int data_len)
{
    unsigned int word;
    int i;
    int word_count;

    if (fnp_state != FNP_STATE_RECEIVING)
    {
        fnp_send_nack(seq, "no transfer in progress");
        return;
    }

    if (data_len <= 0 || (data_len & 3) != 0)
    {
        fnp_send_nack(seq, "data not word-aligned");
        fnp_abort_transfer();
        return;
    }

    /* Update checksum */
    word_count = data_len / 4;
    for (i = 0; i < word_count; i++)
    {
        word = fnp_read_u32(data, i * 4);
        fnp_transfer_checksum = fnp_transfer_checksum + word;
    }

    /* Write raw bytes to file */
    vfs_write(fnp_transfer_gfd, data, data_len);
    fnp_transfer_received = fnp_transfer_received + (unsigned int)word_count;

    fnp_send_ack(seq);
}

static void fnp_handle_file_end(int seq, const char *data, int data_len)
{
    unsigned int expected_checksum;

    if (fnp_state != FNP_STATE_RECEIVING)
    {
        fnp_send_nack(seq, "no transfer in progress");
        return;
    }

    if (data_len < 4)
    {
        fnp_send_nack(seq, "short FILE_END");
        fnp_abort_transfer();
        return;
    }

    expected_checksum = fnp_read_u32(data, 0);

    if (fnp_transfer_received != fnp_transfer_size)
    {
        fnp_send_nack(seq, "size mismatch");
        fnp_abort_transfer();
        return;
    }

    if (fnp_transfer_checksum != expected_checksum)
    {
        fnp_send_nack(seq, "checksum mismatch");
        fnp_abort_transfer();
        return;
    }

    /* Success — close file and sync */
    vfs_close(fnp_transfer_gfd);
    fnp_transfer_gfd = -1;
    fnp_state = FNP_STATE_IDLE;

    term_puts("[FNP] done (");
    term_putint((int)fnp_transfer_received);
    term_puts(" words)\n");

    fnp_transfer_checksum = 0;
    fnp_transfer_size = 0;
    fnp_transfer_received = 0;

    fnp_send_ack(seq);
}

static void fnp_handle_file_abort(int seq)
{
    fnp_abort_transfer();
    fnp_send_ack(seq);
    term_puts("[FNP] transfer aborted\n");
}

static void fnp_handle_keycode(int seq, const char *data, int data_len, int flags)
{
    if (data_len >= 2)
    {
        unsigned int keycode;
        keycode = fnp_read_u16(data, 0);
        hid_event_push(keycode);
    }
    if (flags & FNP_FLAG_REQUIRES_ACK)
        fnp_send_ack(seq);
}

/* ---- Public API ---- */

void fnp_init(void)
{
    fnp_has_peer = 0;
    fnp_state = FNP_STATE_IDLE;
    fnp_transfer_gfd = -1;
    fnp_transfer_checksum = 0;
    fnp_transfer_size = 0;
    fnp_transfer_received = 0;
}

int fnp_poll(void)
{
    int rxlen;
    unsigned int ethertype;
    int type;
    int seq;
    int flags;
    int payload_len;
    const char *payload;

    /* Skip if user program owns the network */
    if (net_user_owned) return 0;

    rxlen = net_ringbuf_pop(fnp_rx, FNP_FRAME_MAX);
    if (rxlen < FNP_HDR_DATA) return 0;

    /* Check EtherType */
    ethertype = fnp_read_u16(fnp_rx, 12);
    if (ethertype != 0xB4B4) return 0;

    /* Learn peer MAC from first FNP packet */
    if (!fnp_has_peer)
    {
        int i;
        for (i = 0; i < 6; i++)
            fnp_peer_mac[i] = fnp_rx[6 + i]; /* Source MAC */
        fnp_has_peer = 1;
    }

    /* Parse FNP header */
    type = (unsigned char)fnp_rx[FNP_HDR_TYPE];
    seq = (int)fnp_read_u16(fnp_rx, FNP_HDR_SEQ);
    flags = (unsigned char)fnp_rx[FNP_HDR_FLAGS];
    payload_len = (int)fnp_read_u16(fnp_rx, FNP_HDR_LENGTH);
    payload = &fnp_rx[FNP_HDR_DATA];

    /* Clamp payload to actual received data */
    if (payload_len > rxlen - FNP_HDR_DATA)
        payload_len = rxlen - FNP_HDR_DATA;

    switch (type)
    {
    case FNP_TYPE_FILE_START:
        fnp_handle_file_start(seq, payload, payload_len);
        break;

    case FNP_TYPE_FILE_DATA:
        fnp_handle_file_data(seq, payload, payload_len);
        break;

    case FNP_TYPE_FILE_END:
        fnp_handle_file_end(seq, payload, payload_len);
        break;

    case FNP_TYPE_FILE_ABORT:
        fnp_handle_file_abort(seq);
        break;

    case FNP_TYPE_KEYCODE:
        fnp_handle_keycode(seq, payload, payload_len, flags);
        break;

    case FNP_TYPE_MESSAGE:
        /* Display text message on terminal */
        if (payload_len > 0)
        {
            term_puts("[FNP] ");
            term_write(payload, payload_len);
            term_putchar('\n');
        }
        if (flags & FNP_FLAG_REQUIRES_ACK)
            fnp_send_ack(seq);
        break;

    default:
        /* Unknown type — ignore */
        break;
    }

    return 1;
}
