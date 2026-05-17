/*
 * net.h — Networking subsystem (FNP protocol).
 *
 * Handles ENC28J60 Ethernet + FNP file transfer protocol.
 * In v4, this will eventually be handled by a netd daemon.
 * For Phase 1, networking remains in-kernel (same as v3).
 */
#ifndef KERNEL_NET_H
#define KERNEL_NET_H

/* Initialize Ethernet + FNP. */
void net_init(void);

/* Main-loop FNP polling. */
void net_poll(void);

/* ISR: drain ENC28J60 RX into kernel ring buffer. */
void net_isr_drain(void);

/* Network packet ring buffer */
int net_ringbuf_count(void);
int net_ringbuf_pop(char *buf, int max_len);
void net_ringbuf_reset(void);

/* State flags */
extern int net_isr_deferred;
extern int net_enc28j60_spi_in_use;
extern int net_user_owned;

/* MAC address */
extern int net_mac[6];

#endif /* KERNEL_NET_H */
