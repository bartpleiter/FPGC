//
// BDOS ethernet module.
//

#include "BDOS/bdos.h"

// Timer callback: drain packets from ENC28J60 into RX ring buffer
void bdos_poll_ethernet()
{
  int next_head;
  int rxlen;

  // Drain up to BDOS_ETH_RX_SLOTS packets per poll
  while (enc28j60_packet_count() > 0)
  {
    next_head = (bdos_eth_rx_head + 1) % BDOS_ETH_RX_SLOTS;
    if (next_head == bdos_eth_rx_tail)
    {
      // Ring buffer full, packets stay in ENC28J60
      break;
    }

    rxlen = enc28j60_packet_receive(
        bdos_eth_rx_buf[bdos_eth_rx_head],
        BDOS_ETH_RX_BUF_SIZE);

    if (rxlen > 0)
    {
      bdos_eth_rx_len[bdos_eth_rx_head] = rxlen;
      bdos_eth_rx_head = next_head;
    }
  }
}
