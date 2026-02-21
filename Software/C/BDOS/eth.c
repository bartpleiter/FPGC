//
// BDOS FNP (FPGC Network Protocol) module.
//
// Implements the FNP protocol on top of the ENC28J60 Ethernet driver.
//

#include "BDOS/bdos.h"

// ---- Internal helpers ----

// Read a big-endian 16-bit value from a char buffer at offset.
static int fnp_read_u16(char *buf, int offset)
{
  return ((buf[offset] & 0xFF) << 8) | (buf[offset + 1] & 0xFF);
}

// Read a big-endian 32-bit value from a char buffer at offset.
static unsigned int fnp_read_u32(char *buf, int offset)
{
  unsigned int val;
  val = ((buf[offset] & 0xFF) << 24) |
        ((buf[offset + 1] & 0xFF) << 16) |
        ((buf[offset + 2] & 0xFF) << 8) |
        (buf[offset + 3] & 0xFF);
  return val;
}

// Write a big-endian 16-bit value into a char buffer at offset.
static void fnp_write_u16(char *buf, int offset, int val)
{
  buf[offset] = (val >> 8) & 0xFF;
  buf[offset + 1] = val & 0xFF;
}

// Write a big-endian 32-bit value into a char buffer at offset.
static void fnp_write_u32(char *buf, int offset, unsigned int val)
{
  buf[offset] = (val >> 24) & 0xFF;
  buf[offset + 1] = (val >> 16) & 0xFF;
  buf[offset + 2] = (val >> 8) & 0xFF;
  buf[offset + 3] = val & 0xFF;
}

// Build and send an FNP frame.
// peer_mac: 6-byte destination MAC (from char array)
// msg_type: FNP message type
// seq: sequence number
// flags: FNP flags
// data: payload data (can be NULL if data_len == 0)
// data_len: payload length in bytes
// Returns 1 on success, 0 on failure.
static int fnp_send_frame(char *peer_mac, int msg_type, int seq,
                          int flags, char *data, int data_len)
{
  int frame_len;
  int i;

  // Ethernet header: dst MAC (6) + src MAC (6) + EtherType (2) = 14 bytes
  // Destination MAC
  i = 0;
  while (i < 6)
  {
    fnp_tx_buf[i] = peer_mac[i];
    i = i + 1;
  }
  // Source MAC
  i = 0;
  while (i < 6)
  {
    fnp_tx_buf[6 + i] = fnp_our_mac[i];
    i = i + 1;
  }
  // EtherType
  fnp_tx_buf[12] = (FNP_ETHERTYPE >> 8) & 0xFF;
  fnp_tx_buf[13] = FNP_ETHERTYPE & 0xFF;

  // FNP header: Ver(1) + Type(1) + Seq(2) + Flags(1) + Length(2) = 7 bytes
  fnp_tx_buf[14] = FNP_VERSION;
  fnp_tx_buf[15] = msg_type;
  fnp_write_u16(fnp_tx_buf, 16, seq);
  fnp_tx_buf[18] = flags;
  fnp_write_u16(fnp_tx_buf, 19, data_len);

  // Payload
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

// Send an ACK for the given sequence number back to the current peer.
static void fnp_send_ack(int acked_seq)
{
  char ack_data[2];

  fnp_write_u16(ack_data, 0, acked_seq);
  fnp_send_frame(fnp_peer_mac, FNP_TYPE_ACK, 0, 0, ack_data, 2);
}

// Send a NACK for the given sequence number with an error code and optional message.
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
    nack_data[len] = 0; // null terminator
    len = len + 1;
  }

  fnp_send_frame(fnp_peer_mac, FNP_TYPE_NACK, 0, 0, nack_data, len);
}

// ---- File transfer helpers ----

// Abort any in-progress file transfer and clean up.
static void fnp_abort_transfer()
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

// ---- Message handlers ----

// Handle FILE_START message.
// data points to the FNP data payload, data_len is its length.
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

  // If already receiving, abort the previous transfer
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

  // Copy path string
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
  uart_putint(file_size);
  uart_puts(" words)\n");

  // Create or truncate the file
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

// Handle FILE_DATA message.
static void fnp_handle_file_data(char *data, int data_len, int seq)
{
  int word_count;
  int i;
  unsigned int word;
  unsigned int word_buf[256]; // Max 1024 bytes = 256 words per chunk
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

  // Data length must be a multiple of 4 (word-packed)
  if ((data_len & 3) != 0)
  {
    fnp_send_nack(seq, FNP_ERR_GENERIC, "Data not word-aligned");
    return;
  }

  word_count = data_len >> 2; // data_len / 4

  if (word_count > 256)
  {
    fnp_send_nack(seq, FNP_ERR_GENERIC, "Chunk too large");
    return;
  }

  // Unpack bytes into words (big-endian) and accumulate checksum
  i = 0;
  while (i < word_count)
  {
    word = fnp_read_u32(data, i * 4);
    word_buf[i] = word;
    fnp_transfer_checksum = fnp_transfer_checksum + word;
    i = i + 1;
  }

  // Write words to BRFS
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

// Handle FILE_END message.
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
  uart_puts(" words, checksum ");
  uart_puthex(fnp_transfer_checksum, 1);
  uart_puts(" vs ");
  uart_puthex(expected_checksum, 1);
  uart_puts("\n");

  // Verify word count
  if (fnp_transfer_received != fnp_transfer_size)
  {
    fnp_send_nack(seq, FNP_ERR_GENERIC, "Incomplete transfer");
    fnp_abort_transfer();
    return;
  }

  // Verify checksum
  if (fnp_transfer_checksum != expected_checksum)
  {
    fnp_send_nack(seq, FNP_ERR_GENERIC, "Checksum mismatch");
    fnp_abort_transfer();
    return;
  }

  // Success: close file
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

// Handle FILE_ABORT message.
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

// Handle KEYCODE message.
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

// ---- Main poll function ----

// Initialize FNP state. Called once during BDOS init.
void bdos_fnp_init()
{
  fnp_transfer_state = FNP_STATE_IDLE;
  fnp_transfer_fd = -1;
  fnp_transfer_checksum = 0;
  fnp_transfer_size = 0;
  fnp_transfer_received = 0;
  fnp_tx_seq = 0;
}

// Poll the ENC28J60 for incoming FNP frames and process them.
// Non-blocking: returns immediately if no packets.
void bdos_fnp_poll()
{
  int rxlen;
  int ethertype;
  int version;
  int msg_type;
  int seq;
  int flags;
  int data_len;
  char *data;

  // Check if any packets are pending
  if (enc28j60_packet_count() == 0)
  {
    return;
  }

  // Receive one frame
  rxlen = enc28j60_packet_receive(fnp_rx_buf, FNP_FRAME_BUF_SIZE);
  if (rxlen <= 0)
  {
    return;
  }

  // Minimum frame: 14 (Ethernet header) + 7 (FNP header) = 21 bytes
  if (rxlen < (FNP_ETH_HEADER_SIZE + FNP_HEADER_SIZE))
  {
    return;
  }

  // Check EtherType
  ethertype = fnp_read_u16(fnp_rx_buf, 12);
  if (ethertype != FNP_ETHERTYPE)
  {
    return; // Not an FNP frame, ignore
  }

  // Save source MAC as peer for replies
  {
    int i;
    i = 0;
    while (i < 6)
    {
      fnp_peer_mac[i] = fnp_rx_buf[6 + i];
      i = i + 1;
    }
  }

  // Parse FNP header (starts at offset 14)
  version = fnp_rx_buf[14] & 0xFF;
  msg_type = fnp_rx_buf[15] & 0xFF;
  seq = fnp_read_u16(fnp_rx_buf, 16);
  flags = fnp_rx_buf[18] & 0xFF;
  data_len = fnp_read_u16(fnp_rx_buf, 19);

  // Version check
  if (version != FNP_VERSION)
  {
    return; // Unknown version, silently drop
  }

  // Verify data_len fits in received frame
  if ((FNP_ETH_HEADER_SIZE + FNP_HEADER_SIZE + data_len) > rxlen)
  {
    return; // Truncated frame
  }

  // Point to data payload
  data = fnp_rx_buf + FNP_ETH_HEADER_SIZE + FNP_HEADER_SIZE;

  // Dispatch by message type
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
      // FPGC receiver doesn't currently wait for ACKs
      break;

    case FNP_TYPE_NACK:
      // FPGC receiver doesn't currently wait for ACKs
      break;

    default:
      // Unknown message type - send NACK if required
      if (flags & FNP_FLAG_REQUIRES_ACK)
      {
        fnp_send_nack(seq, FNP_ERR_GENERIC, "Unknown message type");
      }
      break;
  }
}
