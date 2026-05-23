#include <string.h>
#include <syscall.h>
#include "net.h"
#include "arp.h"

static struct arp_entry arp_table[ARP_TABLE_SIZE];

void arp_table_add(unsigned int ip, const char *mac)
{
    int i;

    /* Update existing */
    for (i = 0; i < ARP_TABLE_SIZE; i++)
    {
        if (arp_table[i].valid && arp_table[i].ip == ip)
        {
            memcpy(arp_table[i].mac, mac, 6);
            return;
        }
    }

    /* Find free slot */
    for (i = 0; i < ARP_TABLE_SIZE; i++)
    {
        if (!arp_table[i].valid)
        {
            arp_table[i].ip = ip;
            memcpy(arp_table[i].mac, mac, 6);
            arp_table[i].valid = 1;
            return;
        }
    }

    /* Overwrite slot 0 */
    arp_table[0].ip = ip;
    memcpy(arp_table[0].mac, mac, 6);
    arp_table[0].valid = 1;
}

char *arp_table_lookup(unsigned int ip)
{
    int i;
    for (i = 0; i < ARP_TABLE_SIZE; i++)
    {
        if (arp_table[i].valid && arp_table[i].ip == ip)
            return arp_table[i].mac;
    }
    return (char *)0;
}

void arp_send_reply(const char *dst_mac, unsigned int dst_ip)
{
    char pkt[ARP_PACKET_LEN];
    int i;

    eth_build(pkt, dst_mac, ETHERTYPE_ARP);

    write_u16(pkt + 14, 1);       /* Hardware type: Ethernet */
    write_u16(pkt + 16, 0x0800);  /* Protocol type: IPv4 */
    pkt[18] = 6;
    pkt[19] = 4;
    write_u16(pkt + 20, ARP_REPLY);

    for (i = 0; i < 6; i++)
        pkt[22 + i] = my_mac[i];
    write_u32(pkt + 28, my_ip);

    for (i = 0; i < 6; i++)
        pkt[32 + i] = dst_mac[i];
    write_u32(pkt + 38, dst_ip);

    sys_net_send(pkt, ARP_PACKET_LEN);
}

void arp_send_request(unsigned int target_ip)
{
    char pkt[ARP_PACKET_LEN];
    char broadcast[6];
    int i;

    memset(broadcast, 0xFF, 6);
    eth_build(pkt, broadcast, ETHERTYPE_ARP);

    write_u16(pkt + 14, 1);
    write_u16(pkt + 16, 0x0800);
    pkt[18] = 6;
    pkt[19] = 4;
    write_u16(pkt + 20, ARP_REQUEST);

    for (i = 0; i < 6; i++)
        pkt[22 + i] = my_mac[i];
    write_u32(pkt + 28, my_ip);

    memset(pkt + 32, 0, 6);
    write_u32(pkt + 38, target_ip);

    sys_net_send(pkt, ARP_PACKET_LEN);
}

void arp_handle(const char *frame, int len)
{
    unsigned int opcode;
    unsigned int sender_ip;
    unsigned int target_ip;

    if (len < 42) return;

    opcode = read_u16(frame + 20);
    sender_ip = read_u32(frame + 28);
    target_ip = read_u32(frame + 38);

    /* Learn sender MAC */
    arp_table_add(sender_ip, frame + 22);

    if (opcode == ARP_REQUEST && target_ip == my_ip)
    {
        arp_send_reply(frame + 22, sender_ip);
    }
}
