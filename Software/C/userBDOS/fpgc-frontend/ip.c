#include <string.h>
#include <syscall.h>
#include "net.h"
#include "arp.h"
#include "ip.h"
#include "tcp.h"

static unsigned int ip_id_counter;

int ip_build(char *frame, unsigned int dst_ip, int protocol, int data_len)
{
    char *ip;
    unsigned int total_len;
    unsigned int cksum;

    ip = frame + ETH_HEADER_LEN;
    total_len = (unsigned int)(IP_HEADER_LEN + data_len);

    ip[0] = 0x45;              /* Version 4, IHL 5 */
    ip[1] = 0x00;
    write_u16(ip + 2, total_len);
    write_u16(ip + 4, ip_id_counter++);
    write_u16(ip + 6, 0x4000); /* Don't fragment */
    ip[8] = 64;                /* TTL */
    ip[9] = (char)protocol;
    write_u16(ip + 10, 0);     /* Checksum placeholder */
    write_u32(ip + 12, my_ip);
    write_u32(ip + 16, dst_ip);

    cksum = ip_checksum(ip, IP_HEADER_LEN);
    write_u16(ip + 10, cksum);

    return ETH_HEADER_LEN + IP_HEADER_LEN;
}

static void handle_icmp(const char *frame, int len, unsigned int src_ip)
{
    char *icmp;
    int icmp_len;
    int ip_offset;
    unsigned int cksum;
    char *dst_mac;
    int i;

    ip_offset = ETH_HEADER_LEN + IP_HEADER_LEN;
    icmp = (char *)(frame + ip_offset);
    icmp_len = len - ip_offset;

    if (icmp_len < 8) return;
    if (icmp[0] != 8) return; /* Only echo request (type 8) */

    dst_mac = arp_table_lookup(src_ip);
    if (!dst_mac) return;

    eth_build(tx_buf, dst_mac, ETHERTYPE_IP);
    ip_build(tx_buf, src_ip, IP_PROTO_ICMP, icmp_len);

    /* Copy ICMP data, change type to 0 (echo reply) */
    for (i = 0; i < icmp_len; i++)
        tx_buf[ip_offset + i] = icmp[i];
    tx_buf[ip_offset] = 0;

    /* Recalculate ICMP checksum */
    write_u16(tx_buf + ip_offset + 2, 0);
    cksum = ip_checksum(tx_buf + ip_offset, icmp_len);
    write_u16(tx_buf + ip_offset + 2, cksum);

    sys_net_send(tx_buf, ip_offset + icmp_len);
}

void ip_handle(const char *frame, int len)
{
    unsigned int src_ip;
    int protocol;

    if (len < ETH_HEADER_LEN + IP_HEADER_LEN) return;

    src_ip = read_u32(frame + ETH_HEADER_LEN + 12);
    protocol = (int)(unsigned char)frame[ETH_HEADER_LEN + 9];

    /* Learn sender MAC for ARP */
    arp_table_add(src_ip, frame + 6);

    if (protocol == IP_PROTO_ICMP)
        handle_icmp(frame, len, src_ip);
    else if (protocol == IP_PROTO_TCP)
        tcp_handle(frame, len, src_ip);
}
