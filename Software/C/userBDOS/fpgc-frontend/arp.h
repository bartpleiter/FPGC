#ifndef ARP_H
#define ARP_H

void arp_table_add(unsigned int ip, const char *mac);
char *arp_table_lookup(unsigned int ip);
void arp_send_reply(const char *dst_mac, unsigned int dst_ip);
void arp_send_request(unsigned int target_ip);
void arp_handle(const char *frame, int len);

#endif
