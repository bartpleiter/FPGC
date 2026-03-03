//
// fnp_test.c — userBDOS test program for FNP networking syscalls.
//
// Sends a test FNP message to the PC (broadcast) and listens for incoming
// frames, printing received message types and data.
//
// Usage: run fnp_test
//

#define USER_SYSCALL
#define USER_FNP
#include "libs/user/user.h"

// Broadcast MAC address for test messages
int broadcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Frame buffer (shared for TX/RX)
char frame_buf[FNP_FRAME_BUF_SIZE];

void print_mac(int *mac)
{
  int i;
  char hex_chars[17] = "0123456789ABCDEF";
  char buf[3];
  buf[2] = 0;

  i = 0;
  while (i < 6)
  {
    if (i > 0)
    {
      sys_print_char(':');
    }
    buf[0] = hex_chars[(mac[i] >> 4) & 0xF];
    buf[1] = hex_chars[mac[i] & 0xF];
    sys_print_str(buf);
    i = i + 1;
  }
}

void print_int(int val)
{
  char buf[12];
  int i;
  int neg;
  unsigned int uval;

  if (val == 0)
  {
    sys_print_char('0');
    return;
  }

  neg = 0;
  if (val < 0)
  {
    neg = 1;
    uval = (unsigned int)(0 - val);
  }
  else
  {
    uval = (unsigned int)val;
  }

  i = 11;
  buf[i] = 0;
  while (uval > 0)
  {
    i = i - 1;
    buf[i] = '0' + (uval % 10);
    uval = uval / 10;
  }

  if (neg)
  {
    i = i - 1;
    buf[i] = '-';
  }

  sys_print_str(buf + i);
}

int main()
{
  int our_mac[6];
  int seq;
  char test_payload[32];
  int i;
  int rxlen;
  int src_mac[6];
  int msg_type;
  int rx_seq;
  int rx_flags;
  char *rx_data;
  int rx_data_len;
  int count;

  // Initialize FNP library
  fnp_init();

  // Print our MAC address
  fnp_get_our_mac(our_mac);
  sys_print_str("FNP Test Program\n");
  sys_print_str("Our MAC: ");
  print_mac(our_mac);
  sys_print_str("\n\n");

  // Build a test payload
  test_payload[0] = 'H';
  test_payload[1] = 'E';
  test_payload[2] = 'L';
  test_payload[3] = 'L';
  test_payload[4] = 'O';
  test_payload[5] = 0;

  // Send a test message (broadcast, message type 0x30)
  sys_print_str("Sending test message (broadcast)...\n");
  seq = 0;
  if (fnp_send(broadcast_mac, FNP_TYPE_MESSAGE, seq, 0,
               test_payload, 6, frame_buf))
  {
    sys_print_str("  Sent OK (seq=0)\n");
  }
  else
  {
    sys_print_str("  Send FAILED\n");
  }

  // Listen for incoming frames for 10 seconds
  sys_print_str("\nListening for FNP frames (10 seconds)...\n");
  count = 0;
  i = 0;
  while (i < 100)  // 100 * 100ms = 10 seconds
  {
    if (sys_net_packet_count() > 0)
    {
      rxlen = sys_net_recv(frame_buf, FNP_FRAME_BUF_SIZE);
      if (rxlen > 0)
      {
        if (fnp_parse(frame_buf, rxlen, src_mac,
                      &msg_type, &rx_seq, &rx_flags,
                      &rx_data, &rx_data_len))
        {
          count = count + 1;
          sys_print_str("  [");
          print_int(count);
          sys_print_str("] From ");
          print_mac(src_mac);
          sys_print_str(" type=0x");
          // Print hex byte
          {
            char hex_chars[17] = "0123456789ABCDEF";
            sys_print_char(hex_chars[(msg_type >> 4) & 0xF]);
            sys_print_char(hex_chars[msg_type & 0xF]);
          }
          sys_print_str(" seq=");
          print_int(rx_seq);
          sys_print_str(" len=");
          print_int(rx_data_len);
          sys_print_str("\n");
        }
        else
        {
          sys_print_str("  (non-FNP frame, ");
          print_int(rxlen);
          sys_print_str(" bytes)\n");
        }
      }
    }
    sys_delay(100);
    i = i + 1;
  }

  sys_print_str("\nReceived ");
  print_int(count);
  sys_print_str(" FNP frame(s) total.\n");
  sys_print_str("Test complete.\n");

  return 0;
}
