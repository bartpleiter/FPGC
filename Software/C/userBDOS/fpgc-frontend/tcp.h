#ifndef TCP_H
#define TCP_H

#include "net.h"

extern struct tcp_conn connections[MAX_CONNECTIONS];

struct tcp_conn *tcp_find_conn(unsigned int remote_ip, int remote_port);
struct tcp_conn *tcp_alloc_conn(void);
void tcp_free_conn(struct tcp_conn *conn);
void tcp_send(struct tcp_conn *conn, int flags, const char *data, int data_len);
void tcp_send_rst(unsigned int remote_ip, int remote_port,
                  int local_port, unsigned int seq, unsigned int ack);
void tcp_handle(const char *frame, int len, unsigned int src_ip);
void tcp_service(void);
void tcp_check_timeouts(void);

#endif
