//
// FNP (FPGC Network Protocol) user-side helper library.
//
// Provides frame building, parsing, and reliable send over raw Ethernet syscalls.
//

#include "libs/user/syscall.h"
#include "libs/user/fnp.h"

// ---- Internal state ----

// Our MAC address, cached at init time.
static int fnp_our_mac[6];

// ---- Internal helpers ----

static void fnp_write_u16(char *buf, int offset, int val)
{
  buf[offset] = (val >> 8) & 0xFF;
  buf[offset + 1] = val & 0xFF;
}

static int fnp_read_u16(char *buf, int offset)
{
  return ((buf[offset] & 0xFF) << 8) | (buf[offset + 1] & 0xFF);
}

// ---- Public API ----

void fnp_init()
{
  sys_net_get_mac(fnp_our_mac);
}

int fnp_send(int *dest_mac, int msg_type, int seq, int flags,
             char *data, int data_len, char *frame_buf)
{
  int frame_len;
  int i;

  if (data_len > FNP_MAX_DATA)
  {
    return 0;
  }

  // Ethernet header: dst MAC (6) + src MAC (6) + EtherType (2) = 14 bytes
  i = 0;
  while (i < 6)
  {
    frame_buf[i] = dest_mac[i];
    frame_buf[6 + i] = fnp_our_mac[i];
    i = i + 1;
  }
  // EtherType
  frame_buf[12] = (FNP_ETHERTYPE >> 8) & 0xFF;
  frame_buf[13] = FNP_ETHERTYPE & 0xFF;

  // FNP header: Ver(1) + Type(1) + Seq(2) + Flags(1) + Length(2) = 7 bytes
  frame_buf[14] = FNP_VERSION;
  frame_buf[15] = msg_type;
  fnp_write_u16(frame_buf, 16, seq);
  frame_buf[18] = flags;
  fnp_write_u16(frame_buf, 19, data_len);

  // Payload
  if (data_len > 0 && data != 0)
  {
    i = 0;
    while (i < data_len)
    {
      frame_buf[FNP_ETH_HEADER_SIZE + FNP_HEADER_SIZE + i] = data[i];
      i = i + 1;
    }
  }

  frame_len = FNP_ETH_HEADER_SIZE + FNP_HEADER_SIZE + data_len;

  return sys_net_send(frame_buf, frame_len);
}

int fnp_parse(char *frame_buf, int frame_len,
              int *src_mac_out, int *msg_type_out, int *seq_out,
              int *flags_out, char **data_out, int *data_len_out)
{
  int ethertype;
  int version;
  int payload_len;
  int i;

  // Minimum frame size: 14 (Eth) + 7 (FNP) = 21 bytes
  if (frame_len < (FNP_ETH_HEADER_SIZE + FNP_HEADER_SIZE))
  {
    return 0;
  }

  // Check EtherType
  ethertype = fnp_read_u16(frame_buf, 12);
  if (ethertype != FNP_ETHERTYPE)
  {
    return 0;
  }

  // Extract source MAC
  i = 0;
  while (i < 6)
  {
    src_mac_out[i] = frame_buf[6 + i] & 0xFF;
    i = i + 1;
  }

  // Parse FNP header
  version = frame_buf[14] & 0xFF;
  if (version != FNP_VERSION)
  {
    return 0;
  }

  *msg_type_out = frame_buf[15] & 0xFF;
  *seq_out = fnp_read_u16(frame_buf, 16);
  *flags_out = frame_buf[18] & 0xFF;
  payload_len = fnp_read_u16(frame_buf, 19);

  // Verify payload fits in frame
  if ((FNP_ETH_HEADER_SIZE + FNP_HEADER_SIZE + payload_len) > frame_len)
  {
    return 0;
  }

  *data_out = frame_buf + FNP_ETH_HEADER_SIZE + FNP_HEADER_SIZE;
  *data_len_out = payload_len;

  return 1;
}

int fnp_send_reliable(int *dest_mac, int msg_type,
                      char *data, int data_len,
                      char *frame_buf, int *seq_counter)
{
  int seq;
  int attempt;
  int wait;
  int rxlen;
  int src_mac[6];
  int rx_msg_type;
  int rx_seq;
  int rx_flags;
  char *rx_data;
  int rx_data_len;
  int acked_seq;

  seq = *seq_counter;
  *seq_counter = seq + 1;

  attempt = 0;
  while (attempt < FNP_MAX_RETRIES)
  {
    // Send the frame with REQUIRES_ACK flag
    if (!fnp_send(dest_mac, msg_type, seq, FNP_FLAG_REQUIRES_ACK,
                  data, data_len, frame_buf))
    {
      attempt = attempt + 1;
      continue;
    }

    // Wait for ACK with timeout
    wait = 0;
    while (wait < FNP_ACK_TIMEOUT_MS)
    {
      if (sys_net_packet_count() > 0)
      {
        rxlen = sys_net_recv(frame_buf, FNP_FRAME_BUF_SIZE);
        if (rxlen > 0 && fnp_parse(frame_buf, rxlen, src_mac,
                                   &rx_msg_type, &rx_seq, &rx_flags,
                                   &rx_data, &rx_data_len))
        {
          if (rx_msg_type == FNP_TYPE_ACK && rx_data_len >= 2)
          {
            acked_seq = fnp_read_u16(rx_data, 0);
            if (acked_seq == seq)
            {
              return 1; // ACK received
            }
          }
        }
      }
      sys_delay(1);
      wait = wait + 1;
    }

    attempt = attempt + 1;
  }

  return 0; // All retries exhausted
}

void fnp_get_our_mac(int *mac_out)
{
  int i;
  i = 0;
  while (i < 6)
  {
    mac_out[i] = fnp_our_mac[i];
    i = i + 1;
  }
}
