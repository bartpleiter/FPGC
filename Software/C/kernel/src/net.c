/*
 * net.c — Networking subsystem stub.
 *
 * Phase 1: minimal ENC28J60 init + ISR-driven RX into ring buffer.
 * Will be expanded to a full netd service in Phase 3.
 */
#include "kernel.h"

/* State */
int net_isr_deferred;
int net_enc28j60_spi_in_use;
int net_user_owned;
int net_mac[6];

/* Ring buffer for received packets */
#define NET_RING_SIZE 4
#define NET_PKT_MAX   1536

static char net_ring[NET_RING_SIZE][NET_PKT_MAX];
static int  net_ring_len[NET_RING_SIZE];
static int  net_ring_head;
static int  net_ring_tail;
static int  net_ring_count;

int net_ringbuf_count(void)
{
    return net_ring_count;
}

int net_ringbuf_pop(char *buf, int max_len)
{
    int len;
    int i;

    if (net_ring_count <= 0) return -1;

    len = net_ring_len[net_ring_tail];
    if (len > max_len) len = max_len;
    for (i = 0; i < len; i++)
        buf[i] = net_ring[net_ring_tail][i];

    net_ring_tail = (net_ring_tail + 1) % NET_RING_SIZE;
    net_ring_count--;
    return len;
}

void net_ringbuf_reset(void)
{
    net_ring_head = 0;
    net_ring_tail = 0;
    net_ring_count = 0;
}

/* ISR: drain ENC28J60 RX into ring buffer */
void net_isr_drain(void)
{
    int pkt_len;

    enc28j60_isr_begin();

    while (enc28j60_packet_count() > 0)
    {
        if (net_ring_count >= NET_RING_SIZE)
        {
            /* Ring full — drain remaining packets to discard */
            char discard[NET_PKT_MAX];
            enc28j60_packet_receive(discard, NET_PKT_MAX);
            continue;
        }

        pkt_len = enc28j60_packet_receive(net_ring[net_ring_head], NET_PKT_MAX);
        if (pkt_len > 0)
        {
            net_ring_len[net_ring_head] = pkt_len;
            net_ring_head = (net_ring_head + 1) % NET_RING_SIZE;
            net_ring_count++;
        }
    }

    enc28j60_isr_end();
}

/* Main-loop polling */
void net_poll(void)
{
    /* For Phase 1, FNP processing is done synchronously in the shell
     * via explicit commands. No polling needed here yet. */
}

void net_init(void)
{
    net_isr_deferred = 0;
    net_enc28j60_spi_in_use = 0;
    net_user_owned = 0;
    net_ringbuf_reset();

    /* Initialize ENC28J60 */
    enc28j60_init(net_mac);
}
