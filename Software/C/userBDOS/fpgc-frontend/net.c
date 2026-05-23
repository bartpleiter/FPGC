#include <string.h>
#include <syscall.h>
#include "net.h"

/* Shared global state */
char my_mac[6];
char tx_buf[MAX_FRAME_LEN];
unsigned int my_ip;
unsigned int my_gateway;

/* Byte read/write (big-endian network order) */
unsigned int read_u16(const char *buf)
{
    return ((unsigned int)(unsigned char)buf[0] << 8) |
           (unsigned int)(unsigned char)buf[1];
}

unsigned int read_u32(const char *buf)
{
    return ((unsigned int)(unsigned char)buf[0] << 24) |
           ((unsigned int)(unsigned char)buf[1] << 16) |
           ((unsigned int)(unsigned char)buf[2] << 8) |
           (unsigned int)(unsigned char)buf[3];
}

void write_u16(char *buf, unsigned int val)
{
    buf[0] = (char)(val >> 8);
    buf[1] = (char)(val & 0xFF);
}

void write_u32(char *buf, unsigned int val)
{
    buf[0] = (char)(val >> 24);
    buf[1] = (char)((val >> 16) & 0xFF);
    buf[2] = (char)((val >> 8) & 0xFF);
    buf[3] = (char)(val & 0xFF);
}

/* IP checksum (ones' complement sum of 16-bit words) */
unsigned int ip_checksum(const char *buf, int len)
{
    unsigned int sum;
    int i;

    sum = 0;
    for (i = 0; i < len - 1; i += 2)
    {
        sum += ((unsigned int)(unsigned char)buf[i] << 8) |
               (unsigned int)(unsigned char)buf[i + 1];
    }
    if (len & 1)
        sum += (unsigned int)(unsigned char)buf[len - 1] << 8;

    while (sum >> 16)
        sum = (sum & 0xFFFF) + (sum >> 16);

    return (~sum) & 0xFFFF;
}

/* TCP checksum with pseudo-header */
unsigned int tcp_checksum(unsigned int src_ip, unsigned int dst_ip,
                          const char *tcp_hdr, int tcp_len)
{
    unsigned int sum;
    int i;

    /* Pseudo-header */
    sum = (src_ip >> 16) + (src_ip & 0xFFFF);
    sum += (dst_ip >> 16) + (dst_ip & 0xFFFF);
    sum += (unsigned int)IP_PROTO_TCP;
    sum += (unsigned int)tcp_len;

    /* TCP header + data */
    for (i = 0; i < tcp_len - 1; i += 2)
    {
        sum += ((unsigned int)(unsigned char)tcp_hdr[i] << 8) |
               (unsigned int)(unsigned char)tcp_hdr[i + 1];
    }
    if (tcp_len & 1)
        sum += (unsigned int)(unsigned char)tcp_hdr[tcp_len - 1] << 8;

    while (sum >> 16)
        sum = (sum & 0xFFFF) + (sum >> 16);

    return (~sum) & 0xFFFF;
}

/* Build Ethernet frame header */
void eth_build(char *frame, const char *dst_mac, unsigned int ethertype)
{
    int i;
    for (i = 0; i < 6; i++)
        frame[i] = dst_mac[i];
    for (i = 0; i < 6; i++)
        frame[6 + i] = my_mac[i];
    write_u16(frame + 12, ethertype);
}
