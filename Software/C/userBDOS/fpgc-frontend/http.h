#ifndef HTTP_H
#define HTTP_H

#include "net.h"

void http_handle_request(struct tcp_conn *conn);
void http_send_header(struct tcp_conn *conn);
void http_send_body_chunk(struct tcp_conn *conn);

#endif
