#include <string.h>
#include <syscall.h>
#include "net.h"
#include "arp.h"
#include "ip.h"
#include "tcp.h"
#include "http.h"

struct tcp_conn connections[MAX_CONNECTIONS];

#define TIMEOUT_US  30000000  /* 30 seconds */

struct tcp_conn *tcp_find_conn(unsigned int remote_ip, int remote_port)
{
    int i;
    for (i = 0; i < MAX_CONNECTIONS; i++)
    {
        if (connections[i].state != STATE_FREE &&
            connections[i].remote_ip == remote_ip &&
            connections[i].remote_port == remote_port)
            return &connections[i];
    }
    return (struct tcp_conn *)0;
}

struct tcp_conn *tcp_alloc_conn(void)
{
    int i;
    for (i = 0; i < MAX_CONNECTIONS; i++)
    {
        if (connections[i].state == STATE_FREE)
            return &connections[i];
    }
    return (struct tcp_conn *)0;
}

void tcp_free_conn(struct tcp_conn *conn)
{
    if (conn->response_fd >= 0)
    {
        sys_close(conn->response_fd);
        conn->response_fd = -1;
    }
    conn->state = STATE_FREE;
}

void tcp_send(struct tcp_conn *conn, int flags, const char *data, int data_len)
{
    char *dst_mac;
    char *tcp_hdr;
    int tcp_len;
    unsigned int cksum;
    int total_len;

    /* Resolve destination MAC */
    if ((conn->remote_ip & 0xFFFFFF00) == (my_ip & 0xFFFFFF00))
        dst_mac = arp_table_lookup(conn->remote_ip);
    else
        dst_mac = arp_table_lookup(my_gateway);

    if (!dst_mac)
    {
        if ((conn->remote_ip & 0xFFFFFF00) == (my_ip & 0xFFFFFF00))
            arp_send_request(conn->remote_ip);
        else
            arp_send_request(my_gateway);
        return;
    }

    eth_build(tx_buf, dst_mac, ETHERTYPE_IP);
    tcp_len = TCP_HEADER_LEN + data_len;
    ip_build(tx_buf, conn->remote_ip, IP_PROTO_TCP, tcp_len);

    tcp_hdr = tx_buf + ETH_HEADER_LEN + IP_HEADER_LEN;

    write_u16(tcp_hdr + 0, (unsigned int)HTTP_PORT);
    write_u16(tcp_hdr + 2, (unsigned int)conn->remote_port);
    write_u32(tcp_hdr + 4, conn->seq_local);
    write_u32(tcp_hdr + 8, conn->seq_remote);
    tcp_hdr[12] = 0x50;  /* Data offset: 5 words */
    tcp_hdr[13] = (char)flags;
    write_u16(tcp_hdr + 14, 4096);  /* Window size */
    write_u16(tcp_hdr + 16, 0);     /* Checksum placeholder */
    write_u16(tcp_hdr + 18, 0);     /* Urgent pointer */

    if (data_len > 0)
        memcpy(tcp_hdr + TCP_HEADER_LEN, data, (unsigned int)data_len);

    cksum = tcp_checksum(my_ip, conn->remote_ip, tcp_hdr, tcp_len);
    write_u16(tcp_hdr + 16, cksum);

    total_len = ETH_HEADER_LEN + IP_HEADER_LEN + tcp_len;
    sys_net_send(tx_buf, total_len);
}

void tcp_send_rst(unsigned int remote_ip, int remote_port,
                  int local_port, unsigned int seq, unsigned int ack)
{
    char *dst_mac;
    char *tcp_hdr;
    unsigned int cksum;
    int tcp_len;

    if ((remote_ip & 0xFFFFFF00) == (my_ip & 0xFFFFFF00))
        dst_mac = arp_table_lookup(remote_ip);
    else
        dst_mac = arp_table_lookup(my_gateway);

    if (!dst_mac) return;

    eth_build(tx_buf, dst_mac, ETHERTYPE_IP);
    tcp_len = TCP_HEADER_LEN;
    ip_build(tx_buf, remote_ip, IP_PROTO_TCP, tcp_len);

    tcp_hdr = tx_buf + ETH_HEADER_LEN + IP_HEADER_LEN;

    write_u16(tcp_hdr + 0, (unsigned int)local_port);
    write_u16(tcp_hdr + 2, (unsigned int)remote_port);
    write_u32(tcp_hdr + 4, seq);
    write_u32(tcp_hdr + 8, ack);
    tcp_hdr[12] = 0x50;
    tcp_hdr[13] = TCP_RST | TCP_ACK;
    write_u16(tcp_hdr + 14, 0);
    write_u16(tcp_hdr + 16, 0);
    write_u16(tcp_hdr + 18, 0);

    cksum = tcp_checksum(my_ip, remote_ip, tcp_hdr, tcp_len);
    write_u16(tcp_hdr + 16, cksum);

    sys_net_send(tx_buf, ETH_HEADER_LEN + IP_HEADER_LEN + tcp_len);
}

void tcp_handle(const char *frame, int len, unsigned int src_ip)
{
    const char *tcp_hdr;
    int src_port;
    int dst_port;
    unsigned int seq;
    unsigned int ack;
    int data_offset;
    int flags;
    int tcp_data_len;
    const char *tcp_data;
    struct tcp_conn *conn;
    int ip_total_len;

    tcp_hdr = frame + ETH_HEADER_LEN + IP_HEADER_LEN;
    ip_total_len = (int)read_u16(frame + ETH_HEADER_LEN + 2);

    src_port = (int)read_u16(tcp_hdr + 0);
    dst_port = (int)read_u16(tcp_hdr + 2);
    seq = read_u32(tcp_hdr + 4);
    ack = read_u32(tcp_hdr + 8);
    data_offset = ((tcp_hdr[12] >> 4) & 0x0F) * 4;
    flags = tcp_hdr[13] & 0x3F;

    tcp_data_len = ip_total_len - IP_HEADER_LEN - data_offset;
    if (tcp_data_len < 0) tcp_data_len = 0;
    tcp_data = tcp_hdr + data_offset;

    /* Only accept connections to our HTTP port */
    if (dst_port != HTTP_PORT)
    {
        tcp_send_rst(src_ip, src_port, dst_port, ack, seq + 1);
        return;
    }

    conn = tcp_find_conn(src_ip, src_port);

    if (!conn)
    {
        if (!(flags & TCP_SYN))
        {
            tcp_send_rst(src_ip, src_port, dst_port, ack, seq + 1);
            return;
        }

        conn = tcp_alloc_conn();
        if (!conn)
        {
            tcp_send_rst(src_ip, src_port, dst_port, 0, seq + 1);
            return;
        }

        conn->state = STATE_SYN_RCVD;
        conn->remote_ip = src_ip;
        conn->remote_port = src_port;
        conn->seq_remote = seq + 1;
        conn->seq_local = (unsigned int)sys_get_time_us();
        conn->http_state = HTTP_RECV_REQ;
        conn->request_len = 0;
        conn->response_fd = -1;
        conn->last_activity = (unsigned int)sys_get_time_us();

        /* Send SYN+ACK */
        tcp_send(conn, TCP_SYN | TCP_ACK, (char *)0, 0);
        conn->seq_local += 1;
        return;
    }

    conn->last_activity = (unsigned int)sys_get_time_us();

    if (flags & TCP_RST)
    {
        tcp_free_conn(conn);
        return;
    }

    switch (conn->state)
    {
    case STATE_SYN_RCVD:
        if (flags & TCP_ACK)
            conn->state = STATE_ESTABLISHED;
        break;

    case STATE_ESTABLISHED:
        if (tcp_data_len > 0)
        {
            if (seq == conn->seq_remote)
            {
                if (conn->http_state == HTTP_RECV_REQ)
                {
                    int space;
                    space = HTTP_REQ_SIZE - conn->request_len - 1;
                    if (tcp_data_len < space)
                        space = tcp_data_len;
                    memcpy(conn->request + conn->request_len,
                           tcp_data, (unsigned int)space);
                    conn->request_len += space;
                    conn->request[conn->request_len] = '\0';
                }
                conn->seq_remote += (unsigned int)tcp_data_len;
                tcp_send(conn, TCP_ACK, (char *)0, 0);

                if (conn->http_state == HTTP_RECV_REQ &&
                    strstr(conn->request, "\r\n\r\n"))
                {
                    http_handle_request(conn);
                }
            }
        }

        if (flags & TCP_FIN)
        {
            conn->seq_remote += 1;
            tcp_send(conn, TCP_ACK | TCP_FIN, (char *)0, 0);
            conn->seq_local += 1;
            tcp_free_conn(conn);
        }
        break;

    case STATE_FIN_WAIT:
        if (flags & TCP_ACK)
        {
            if (flags & TCP_FIN)
            {
                conn->seq_remote += 1;
                tcp_send(conn, TCP_ACK, (char *)0, 0);
            }
            tcp_free_conn(conn);
        }
        else if (flags & TCP_FIN)
        {
            conn->seq_remote += 1;
            tcp_send(conn, TCP_ACK, (char *)0, 0);
            tcp_free_conn(conn);
        }
        break;
    }
}

void tcp_service(void)
{
    int i;
    struct tcp_conn *conn;

    for (i = 0; i < MAX_CONNECTIONS; i++)
    {
        conn = &connections[i];
        if (conn->state != STATE_ESTABLISHED) continue;

        if (conn->http_state == HTTP_SEND_HDR)
            http_send_header(conn);
        else if (conn->http_state == HTTP_SEND_BODY)
            http_send_body_chunk(conn);
    }
}

void tcp_check_timeouts(void)
{
    int i;
    unsigned int now;
    struct tcp_conn *conn;

    now = (unsigned int)sys_get_time_us();

    for (i = 0; i < MAX_CONNECTIONS; i++)
    {
        conn = &connections[i];
        if (conn->state == STATE_FREE) continue;
        /* Skip SSE connections — they're long-lived */
        if (conn->http_state == HTTP_SSE) continue;

        if ((now - conn->last_activity) > TIMEOUT_US)
        {
            tcp_send(conn, TCP_ACK | TCP_FIN, (char *)0, 0);
            conn->seq_local += 1;
            conn->state = STATE_FIN_WAIT;
        }
    }
}
