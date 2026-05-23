#ifndef IP_H
#define IP_H

int ip_build(char *frame, unsigned int dst_ip, int protocol, int data_len);
void ip_handle(const char *frame, int len);

#endif
