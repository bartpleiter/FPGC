/*
 * eth.c — BDOS v3 FNP (FPGC Network Protocol) module.
 *
 * Implements the FNP protocol on top of the ENC28J60 Ethernet driver.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "bdos.h"

/* FNP state variables (declared extern in bdos_fnp.h) */
char fnp_rx_buf[FNP_FRAME_BUF_SIZE];
char fnp_tx_buf[FNP_FRAME_BUF_SIZE];
int fnp_our_mac[6];
char fnp_peer_mac[6];
int fnp_tx_seq = 0;
int fnp_transfer_state = 0;
int fnp_transfer_fd = -1;
unsigned int fnp_transfer_checksum = 0;
unsigned int fnp_transfer_size = 0;
unsigned int fnp_transfer_received = 0;
int fnp_net_user_owned = 0;

/* Ring buffer state */
char net_ringbuf_data[NET_RINGBUF_SLOTS * NET_RINGBUF_FRAME_SIZE];
int net_ringbuf_len[NET_RINGBUF_SLOTS];
int net_ringbuf_head = 0;
int net_ringbuf_tail = 0;
int net_isr_deferred = 0;

/* ---- Internal helpers ---- */

static int fnp_read_u16(char *buf, int offset)
{
  return ((buf[offset] & 0xFF) << 8) | (buf[offset + 1] & 0xFF);
}

static unsigned int fnp_read_u32(char *buf, int offset)
{
  unsigned int val;
  val = ((buf[offset] & 0xFF) << 24) |
        ((buf[offset + 1] & 0xFF) << 16) |
        ((buf[offset + 2] & 0xFF) << 8) |
        (buf[offset + 3] & 0xFF);
  return val;
}

static void fnp_write_u16(char *buf, int offset, int val)
{
  buf[offset] = (val >> 8) & 0xFF;
  buf[offset + 1] = val & 0xFF;
}

static void fnp_write_u32(char *buf, int offset, unsigned int val)
{
  buf[offset] = (val >> 24) & 0xFF;
  buf[offset + 1] = (val >> 16) & 0xFF;
  buf[offset + 2] = (val >> 8) & 0xFF;
  buf[offset + 3] = val & 0xFF;
}

static int fnp_send_frame(char *peer_mac, int msg_type, int seq,
                          int flags, char *data, int data_len)
{
  int frame_len;
  int i;

  i = 0;
  while (i < 6)
  {
    fnp_tx_buf[i] = peer_mac[i];
    i = i + 1;
  }
  i = 0;
  while (i < 6)
  {
    fnp_tx_buf[6 + i] = fnp_our_mac[i];
    i = i + 1;
  }
  fnp_tx_buf[12] = (FNP_ETHERTYPE >> 8) & 0xFF;
  fnp_tx_buf[13] = FNP_ETHERTYPE & 0xFF;

  fnp_tx_buf[14] = FNP_VERSION;
  fnp_tx_buf[15] = msg_type;
  fnp_write_u16(fnp_tx_buf, 16, seq);
  fnp_tx_buf[18] = flags;
  fnp_write_u16(fnp_tx_buf, 19, data_len);

  if (data_len > 0 && data != 0)
  {
    i = 0;
    while (i < data_len)
    {
      fnp_tx_buf[FNP_ETH_HEADER_SIZE + FNP_HEADER_SIZE + i] = data[i];
      i = i + 1;
    }
  }

  frame_len = FNP_ETH_HEADER_SIZE + FNP_HEADER_SIZE + data_len;

  return enc28j60_packet_send(fnp_tx_buf, frame_len);
}

static void fnp_send_ack(int acked_seq)
{
  char ack_data[2];

  fnp_write_u16(ack_data, 0, acked_seq);
  fnp_send_frame(fnp_peer_mac, FNP_TYPE_ACK, 0, 0, ack_data, 2);
}

static void fnp_send_nack(int nacked_seq, int error_code, char *error_msg)
{
  char nack_data[128];
  int len;
  int i;

  fnp_write_u16(nack_data, 0, nacked_seq);
  nack_data[2] = error_code;
  len = 3;

  if (error_msg != 0)
  {
    i = 0;
    while (error_msg[i] != 0 && len < 127)
    {
      nack_data[len] = error_msg[i];
      len = len + 1;
      i = i + 1;
    }
    nack_data[len] = 0;
    len = len + 1;
  }

  fnp_send_frame(fnp_peer_mac, FNP_TYPE_NACK, 0, 0, nack_data, len);
}

/* ---- File transfer helpers ---- */

static void fnp_abort_transfer(void)
{
  if (fnp_transfer_state == FNP_STATE_RECEIVING)
  {
    if (fnp_transfer_fd >= 0)
    {
      brfs_close(fnp_transfer_fd);
      fnp_transfer_fd = -1;
    }
    fnp_transfer_state = FNP_STATE_IDLE;
    fnp_transfer_checksum = 0;
    fnp_transfer_size = 0;
    fnp_transfer_received = 0;
  }
}

/* ---- Message handlers ---- */

static void fnp_handle_file_start(char *data, int data_len, int seq)
{
  int path_len;
  unsigned int file_size;
  char path_buf[BRFS_MAX_PATH_LENGTH + 1];
  int i;
  int fd;

  if (data_len < 7)
  {
    fnp_send_nack(seq, FNP_ERR_GENERIC, "FILE_START too short");
    return;
  }

  if (fnp_transfer_state == FNP_STATE_RECEIVING)
  {
    fnp_abort_transfer();
    uart_puts("[FNP] Aborted stale transfer for new FILE_START\n");
  }

  path_len = fnp_read_u16(data, 0);
  file_size = fnp_read_u32(data, 2);

  if (path_len <= 0 || path_len > BRFS_MAX_PATH_LENGTH)
  {
    fnp_send_nack(seq, FNP_ERR_GENERIC, "Invalid path length");
    return;
  }

  if ((6 + path_len) > data_len)
  {
    fnp_send_nack(seq, FNP_ERR_GENERIC, "Path exceeds payload");
    return;
  }

  i = 0;
  while (i < path_len && i < BRFS_MAX_PATH_LENGTH)
  {
    path_buf[i] = data[6 + i];
    i = i + 1;
  }
  path_buf[i] = 0;

  uart_puts("[FNP] FILE_START: ");
  uart_puts(path_buf);
  uart_puts(" (");
  uart_putint(file_size * 4);
  uart_puts(" bytes)\n");

  if (brfs_exists(path_buf))
  {
    brfs_delete(path_buf);
  }

  if (brfs_create_file(path_buf) < 0)
  {
    fnp_send_nack(seq, FNP_ERR_GENERIC, "Cannot create file");
    return;
  }

  fd = brfs_open(path_buf);
  if (fd < 0)
  {
    fnp_send_nack(seq, FNP_ERR_GENERIC, "Cannot open file");
    return;
  }

  fnp_transfer_fd = fd;
  fnp_transfer_state = FNP_STATE_RECEIVING;
  fnp_transfer_checksum = 0;
  fnp_transfer_size = file_size;
  fnp_transfer_received = 0;

  fnp_send_ack(seq);
}

static void fnp_handle_file_data(char *data, int data_len, int seq)
{
  int word_count;
  int i;
  unsigned int word;
  unsigned int word_buf[256];
  int write_result;

  if (fnp_transfer_state != FNP_STATE_RECEIVING)
  {
    fnp_send_nack(seq, FNP_ERR_GENERIC, "No transfer in progress");
    return;
  }

  if (fnp_transfer_fd < 0)
  {
    fnp_send_nack(seq, FNP_ERR_GENERIC, "File not open");
    return;
  }

  if ((data_len & 3) != 0)
  {
    fnp_send_nack(seq, FNP_ERR_GENERIC, "Data not word-aligned");
    return;
  }

  word_count = data_len >> 2;

  if (word_count > 256)
  {
    fnp_send_nack(seq, FNP_ERR_GENERIC, "Chunk too large");
    return;
  }

  i = 0;
  while (i < word_count)
  {
    word = fnp_read_u32(data, i * 4);
    word_buf[i] = word;
    fnp_transfer_checksum = fnp_transfer_checksum + word;
    i = i + 1;
  }

  write_result = brfs_write(fnp_transfer_fd, word_buf, word_count);
  if (write_result < 0)
  {
    fnp_send_nack(seq, FNP_ERR_GENERIC, "Write failed");
    fnp_abort_transfer();
    return;
  }

  fnp_transfer_received = fnp_transfer_received + word_count;

  fnp_send_ack(seq);
}

static void fnp_handle_file_end(char *data, int data_len, int seq)
{
  unsigned int expected_checksum;

  if (fnp_transfer_state != FNP_STATE_RECEIVING)
  {
    fnp_send_nack(seq, FNP_ERR_GENERIC, "No transfer in progress");
    return;
  }

  if (data_len < 4)
  {
    fnp_send_nack(seq, FNP_ERR_GENERIC, "FILE_END too short");
    return;
  }

  expected_checksum = fnp_read_u32(data, 0);

  uart_puts("[FNP] FILE_END: received ");
  uart_putint(fnp_transfer_received);
  uart_puts("/");
  uart_putint(fnp_transfer_size);
  uart_puts(" packed words, checksum ");
  uart_puthex(fnp_transfer_checksum, 1);
  uart_puts(" vs ");
  uart_puthex(expected_checksum, 1);
  uart_puts("\n");

  if (fnp_transfer_received != fnp_transfer_size)
  {
    fnp_send_nack(seq, FNP_ERR_GENERIC, "Incomplete transfer");
    fnp_abort_transfer();
    return;
  }

  if (fnp_transfer_checksum != expected_checksum)
  {
    fnp_send_nack(seq, FNP_ERR_GENERIC, "Checksum mismatch");
    fnp_abort_transfer();
    return;
  }

  if (fnp_transfer_fd >= 0)
  {
    brfs_close(fnp_transfer_fd);
    fnp_transfer_fd = -1;
  }

  fnp_transfer_state = FNP_STATE_IDLE;
  fnp_transfer_checksum = 0;
  fnp_transfer_size = 0;
  fnp_transfer_received = 0;

  uart_puts("[FNP] File transfer complete\n");

  fnp_send_ack(seq);
}

static void fnp_handle_file_abort(int seq)
{
  if (fnp_transfer_state != FNP_STATE_RECEIVING)
  {
    fnp_send_nack(seq, FNP_ERR_GENERIC, "No transfer to abort");
    return;
  }

  uart_puts("[FNP] Transfer aborted by sender\n");
  fnp_abort_transfer();
  fnp_send_ack(seq);
}

static void fnp_handle_keycode(char *data, int data_len, int seq, int flags)
{
  int keycode;

  if (data_len < 2)
  {
    if (flags & FNP_FLAG_REQUIRES_ACK)
    {
      fnp_send_nack(seq, FNP_ERR_GENERIC, "KEYCODE too short");
    }
    return;
  }

  keycode = fnp_read_u16(data, 0);

  if (!bdos_keyboard_event_fifo_push(keycode))
  {
    if (flags & FNP_FLAG_REQUIRES_ACK)
    {
      fnp_send_nack(seq, FNP_ERR_GENERIC, "HID FIFO full");
    }
    return;
  }

  if (flags & FNP_FLAG_REQUIRES_ACK)
  {
    fnp_send_ack(seq);
  }
}

/* ---- Ring buffer operations ---- */

void bdos_net_isr_drain(void)
{
  int idx;
  int next;
  int len;

  enc28j60_isr_begin();

  while (enc28j60_packet_count() > 0)
  {
    idx = net_ringbuf_head;
    next = (idx + 1) & (NET_RINGBUF_SLOTS - 1);

    if (next == net_ringbuf_tail)
    {
      net_ringbuf_tail = (net_ringbuf_tail + 1) & (NET_RINGBUF_SLOTS - 1);
    }

    len = enc28j60_packet_receive(
      &net_ringbuf_data[idx * NET_RINGBUF_FRAME_SIZE],
      NET_RINGBUF_FRAME_SIZE);
    net_ringbuf_len[idx] = len;
    net_ringbuf_head = next;
  }

  enc28j60_isr_end();
}

void bdos_net_ringbuf_reset(void)
{
  net_ringbuf_head = 0;
  net_ringbuf_tail = 0;
  net_isr_deferred = 0;
}

int bdos_net_ringbuf_pop(char *buf, int max_len)
{
  int idx;
  int len;
  int i;

  if (net_ringbuf_head == net_ringbuf_tail)
  {
    return 0;
  }

  idx = net_ringbuf_tail;
  len = net_ringbuf_len[idx];
  if (len > max_len)
  {
    len = max_len;
  }

  {
    char *src;
    src = &net_ringbuf_data[idx * NET_RINGBUF_FRAME_SIZE];
    i = 0;
    while (i < len)
    {
      buf[i] = src[i];
      i = i + 1;
    }
  }

  net_ringbuf_tail = (idx + 1) & (NET_RINGBUF_SLOTS - 1);
  return len;
}

int bdos_net_ringbuf_count(void)
{
  return (net_ringbuf_head - net_ringbuf_tail + NET_RINGBUF_SLOTS) & (NET_RINGBUF_SLOTS - 1);
}

/* ---- Init and poll ---- */

void bdos_fnp_init(void)
{
  fnp_transfer_state = FNP_STATE_IDLE;
  fnp_transfer_fd = -1;
  fnp_transfer_checksum = 0;
  fnp_transfer_size = 0;
  fnp_transfer_received = 0;
  fnp_tx_seq = 0;
}

void bdos_fnp_poll(void)
{
  int rxlen;
  int ethertype;
  int version;
  int msg_type;
  int seq;
  int flags;
  int data_len;
  char *data;

  if (fnp_net_user_owned)
  {
    return;
  }

  rxlen = bdos_net_ringbuf_pop(fnp_rx_buf, FNP_FRAME_BUF_SIZE);
  if (rxlen <= 0)
  {
    return;
  }

  if (rxlen < (FNP_ETH_HEADER_SIZE + FNP_HEADER_SIZE))
  {
    return;
  }

  ethertype = fnp_read_u16(fnp_rx_buf, 12);
  if (ethertype != FNP_ETHERTYPE)
  {
    return;
  }

  {
    int i;
    i = 0;
    while (i < 6)
    {
      fnp_peer_mac[i] = fnp_rx_buf[6 + i];
      i = i + 1;
    }
  }

  version = fnp_rx_buf[14] & 0xFF;
  msg_type = fnp_rx_buf[15] & 0xFF;
  seq = fnp_read_u16(fnp_rx_buf, 16);
  flags = fnp_rx_buf[18] & 0xFF;
  data_len = fnp_read_u16(fnp_rx_buf, 19);

  if (version != FNP_VERSION)
  {
    return;
  }

  if ((FNP_ETH_HEADER_SIZE + FNP_HEADER_SIZE + data_len) > rxlen)
  {
    return;
  }

  data = fnp_rx_buf + FNP_ETH_HEADER_SIZE + FNP_HEADER_SIZE;

  switch (msg_type)
  {
    case FNP_TYPE_FILE_START:
      fnp_handle_file_start(data, data_len, seq);
      break;

    case FNP_TYPE_FILE_DATA:
      fnp_handle_file_data(data, data_len, seq);
      break;

    case FNP_TYPE_FILE_END:
      fnp_handle_file_end(data, data_len, seq);
      break;

    case FNP_TYPE_FILE_ABORT:
      fnp_handle_file_abort(seq);
      break;

    case FNP_TYPE_KEYCODE:
      fnp_handle_keycode(data, data_len, seq, flags);
      break;

    case FNP_TYPE_ACK:
      break;

    case FNP_TYPE_NACK:
      break;

    default:
      if (flags & FNP_FLAG_REQUIRES_ACK)
      {
        fnp_send_nack(seq, FNP_ERR_GENERIC, "Unknown message type");
      }
      break;
  }
}
