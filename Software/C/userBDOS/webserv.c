/*
 * webserv.c — Minimal HTTP web server for FPGC/BDOS.
 *
 * Implements a lightweight TCP/IP stack over raw Ethernet syscalls
 * and serves static files from /www/ via HTTP/1.0.
 *
 * Static IP: 192.168.0.250, port 80.
 * Only GET requests supported.
 */

#include <syscall.h>
#include <string.h>
#include <stdlib.h>

/* ---- Network configuration ---- */
#define MY_IP_0         192
#define MY_IP_1         168
#define MY_IP_2         0
#define MY_IP_3         250
#define MY_NETMASK_0    255
#define MY_NETMASK_1    255
#define MY_NETMASK_2    255
#define MY_NETMASK_3    0
#define MY_GATEWAY_0    192
#define MY_GATEWAY_1    168
#define MY_GATEWAY_2    0
#define MY_GATEWAY_3    1
#define HTTP_PORT       80

/* ---- Protocol constants ---- */
#define ETH_HEADER_LEN  14
#define IP_HEADER_LEN   20
#define TCP_HEADER_LEN  20
#define TOTAL_HEADER    (ETH_HEADER_LEN + IP_HEADER_LEN + TCP_HEADER_LEN)
#define MAX_FRAME_LEN   1518
#define MAX_TCP_DATA    (MAX_FRAME_LEN - TOTAL_HEADER)
#define ARP_PACKET_LEN  42

/* EtherType */
#define ETHERTYPE_IP    0x0800
#define ETHERTYPE_ARP   0x0806

/* IP protocol */
#define IP_PROTO_ICMP   1
#define IP_PROTO_TCP    6

/* TCP flags */
#define TCP_FIN         0x01
#define TCP_SYN         0x02
#define TCP_RST         0x04
#define TCP_PSH         0x08
#define TCP_ACK         0x10

/* ARP opcodes */
#define ARP_REQUEST     1
#define ARP_REPLY       2

/* TCP connection states */
#define STATE_FREE      0
#define STATE_LISTEN    1
#define STATE_SYN_RCVD  2
#define STATE_ESTABLISHED 3
#define STATE_FIN_WAIT  4
#define STATE_CLOSED    5

/* HTTP states */
#define HTTP_RECV_REQ   0
#define HTTP_SEND_HDR   1
#define HTTP_SEND_BODY  2
#define HTTP_DONE       3

/* ---- ARP table ---- */
#define ARP_TABLE_SIZE  8

struct arp_entry {
    unsigned int ip;
    char mac[6];
    int valid;
};

static struct arp_entry arp_table[ARP_TABLE_SIZE];

/* ---- TCP connections ---- */
#define MAX_CONNECTIONS 4
#define HTTP_REQ_SIZE   512
#define HTTP_PATH_SIZE  128

struct tcp_conn {
    int state;
    unsigned int remote_ip;
    int remote_port;
    unsigned int seq_local;
    unsigned int seq_remote;
    int http_state;
    char request[HTTP_REQ_SIZE];
    int request_len;
    char path[HTTP_PATH_SIZE];
    int response_fd;
    int response_size;
    int response_sent;
    unsigned int last_activity;
};

static struct tcp_conn connections[MAX_CONNECTIONS];

/* ---- Global state ---- */
static char my_mac[6];
static char frame_buf[MAX_FRAME_LEN];
static char tx_buf[MAX_FRAME_LEN];

/* Our IP as a 32-bit value (big-endian) */
static unsigned int my_ip;
static unsigned int my_gateway;

/* ---- Helpers: byte manipulation ---- */

static unsigned int read_u16(const char *buf)
{
    return ((unsigned int)(buf[0] & 0xFF) << 8) |
           ((unsigned int)(buf[1] & 0xFF));
}

static unsigned int read_u32(const char *buf)
{
    return ((unsigned int)(buf[0] & 0xFF) << 24) |
           ((unsigned int)(buf[1] & 0xFF) << 16) |
           ((unsigned int)(buf[2] & 0xFF) << 8) |
           ((unsigned int)(buf[3] & 0xFF));
}

static void write_u16(char *buf, unsigned int val)
{
    buf[0] = (char)((val >> 8) & 0xFF);
    buf[1] = (char)(val & 0xFF);
}

static void write_u32(char *buf, unsigned int val)
{
    buf[0] = (char)((val >> 24) & 0xFF);
    buf[1] = (char)((val >> 16) & 0xFF);
    buf[2] = (char)((val >> 8) & 0xFF);
    buf[3] = (char)(val & 0xFF);
}

/* ---- IP checksum ---- */

static unsigned int ip_checksum(const char *buf, int len)
{
    unsigned int sum;
    int i;

    sum = 0;
    for (i = 0; i + 1 < len; i += 2)
    {
        sum += read_u16(buf + i);
    }
    if (len & 1)
        sum += (unsigned int)(buf[len - 1] & 0xFF) << 8;

    while (sum >> 16)
        sum = (sum & 0xFFFF) + (sum >> 16);

    return (~sum) & 0xFFFF;
}

/* ---- TCP pseudo-header checksum ---- */

static unsigned int tcp_checksum(unsigned int src_ip, unsigned int dst_ip,
                                 const char *tcp_hdr, int tcp_len)
{
    unsigned int sum;
    int i;

    /* Pseudo-header */
    sum = (src_ip >> 16) & 0xFFFF;
    sum += src_ip & 0xFFFF;
    sum += (dst_ip >> 16) & 0xFFFF;
    sum += dst_ip & 0xFFFF;
    sum += IP_PROTO_TCP;
    sum += (unsigned int)tcp_len;

    /* TCP header + data */
    for (i = 0; i + 1 < tcp_len; i += 2)
    {
        sum += read_u16(tcp_hdr + i);
    }
    if (tcp_len & 1)
        sum += (unsigned int)(tcp_hdr[tcp_len - 1] & 0xFF) << 8;

    while (sum >> 16)
        sum = (sum & 0xFFFF) + (sum >> 16);

    return (~sum) & 0xFFFF;
}

/* ---- Ethernet frame building ---- */

static void eth_build(char *frame, const char *dst_mac, unsigned int ethertype)
{
    int i;
    for (i = 0; i < 6; i++)
        frame[i] = dst_mac[i];
    for (i = 0; i < 6; i++)
        frame[6 + i] = my_mac[i];
    write_u16(frame + 12, ethertype);
}

/* ---- ARP ---- */

static void arp_table_add(unsigned int ip, const char *mac)
{
    int i;
    int oldest;

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
    oldest = 0;
    arp_table[oldest].ip = ip;
    memcpy(arp_table[oldest].mac, mac, 6);
    arp_table[oldest].valid = 1;
}

static char *arp_table_lookup(unsigned int ip)
{
    int i;
    for (i = 0; i < ARP_TABLE_SIZE; i++)
    {
        if (arp_table[i].valid && arp_table[i].ip == ip)
            return arp_table[i].mac;
    }
    return (char *)0;
}

static void arp_send_reply(const char *dst_mac, unsigned int dst_ip)
{
    char pkt[ARP_PACKET_LEN];
    int i;

    eth_build(pkt, dst_mac, ETHERTYPE_ARP);

    /* ARP header */
    write_u16(pkt + 14, 1);       /* Hardware type: Ethernet */
    write_u16(pkt + 16, 0x0800);  /* Protocol type: IPv4 */
    pkt[18] = 6;                   /* HW addr len */
    pkt[19] = 4;                   /* Proto addr len */
    write_u16(pkt + 20, ARP_REPLY);

    /* Sender MAC + IP */
    for (i = 0; i < 6; i++)
        pkt[22 + i] = my_mac[i];
    write_u32(pkt + 28, my_ip);

    /* Target MAC + IP */
    for (i = 0; i < 6; i++)
        pkt[32 + i] = dst_mac[i];
    write_u32(pkt + 38, dst_ip);

    sys_net_send(pkt, ARP_PACKET_LEN);
}

static void arp_send_request(unsigned int target_ip)
{
    char pkt[ARP_PACKET_LEN];
    char broadcast[6];
    int i;

    memset(broadcast, 0xFF, 6);
    eth_build(pkt, broadcast, ETHERTYPE_ARP);

    write_u16(pkt + 14, 1);       /* Hardware type: Ethernet */
    write_u16(pkt + 16, 0x0800);  /* Protocol type: IPv4 */
    pkt[18] = 6;
    pkt[19] = 4;
    write_u16(pkt + 20, ARP_REQUEST);

    /* Sender */
    for (i = 0; i < 6; i++)
        pkt[22 + i] = my_mac[i];
    write_u32(pkt + 28, my_ip);

    /* Target */
    memset(pkt + 32, 0, 6);
    write_u32(pkt + 38, target_ip);

    sys_net_send(pkt, ARP_PACKET_LEN);
}

static void handle_arp(const char *frame, int len)
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

/* ---- IP ---- */

static int ip_build(char *frame, unsigned int dst_ip, int protocol, int data_len)
{
    char *ip;
    unsigned int total_len;
    static unsigned int ip_id_counter;
    unsigned int cksum;

    ip = frame + ETH_HEADER_LEN;
    total_len = (unsigned int)(IP_HEADER_LEN + data_len);

    ip[0] = 0x45;              /* Version 4, IHL 5 */
    ip[1] = 0x00;              /* DSCP/ECN */
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

/* ---- ICMP ---- */

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
    if (icmp[0] != 8) return; /* Only handle echo request (type 8) */

    /* Build echo reply */
    dst_mac = arp_table_lookup(src_ip);
    if (!dst_mac) return;

    eth_build(tx_buf, dst_mac, ETHERTYPE_IP);
    ip_build(tx_buf, src_ip, IP_PROTO_ICMP, icmp_len);

    /* Copy ICMP data, change type to 0 (echo reply) */
    for (i = 0; i < icmp_len; i++)
        tx_buf[ip_offset + i] = icmp[i];
    tx_buf[ip_offset] = 0;  /* Type = echo reply */

    /* Recalculate ICMP checksum */
    write_u16(tx_buf + ip_offset + 2, 0);
    cksum = ip_checksum(tx_buf + ip_offset, icmp_len);
    write_u16(tx_buf + ip_offset + 2, cksum);

    sys_net_send(tx_buf, ip_offset + icmp_len);
}

/* ---- TCP ---- */

static struct tcp_conn *tcp_find_conn(unsigned int remote_ip, int remote_port)
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

static struct tcp_conn *tcp_alloc_conn(void)
{
    int i;
    for (i = 0; i < MAX_CONNECTIONS; i++)
    {
        if (connections[i].state == STATE_FREE)
            return &connections[i];
    }
    return (struct tcp_conn *)0;
}

static void tcp_free_conn(struct tcp_conn *conn)
{
    if (conn->response_fd >= 0)
    {
        sys_close(conn->response_fd);
        conn->response_fd = -1;
    }
    conn->state = STATE_FREE;
}

static void tcp_send(struct tcp_conn *conn, int flags,
                     const char *data, int data_len)
{
    char *dst_mac;
    char *tcp_hdr;
    int tcp_len;
    unsigned int cksum;
    int total_len;

    /* Resolve destination MAC */
    /* If on same subnet, use direct MAC; else use gateway */
    if ((conn->remote_ip & 0xFFFFFF00) == (my_ip & 0xFFFFFF00))
        dst_mac = arp_table_lookup(conn->remote_ip);
    else
        dst_mac = arp_table_lookup(my_gateway);

    if (!dst_mac)
    {
        /* Send ARP request and drop this packet */
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

    write_u16(tcp_hdr + 0, (unsigned int)HTTP_PORT);     /* Src port */
    write_u16(tcp_hdr + 2, (unsigned int)conn->remote_port); /* Dst port */
    write_u32(tcp_hdr + 4, conn->seq_local);             /* Seq */
    write_u32(tcp_hdr + 8, conn->seq_remote);            /* Ack */
    tcp_hdr[12] = 0x50;                                  /* Data offset: 5 words */
    tcp_hdr[13] = (char)flags;
    write_u16(tcp_hdr + 14, 4096);                       /* Window size */
    write_u16(tcp_hdr + 16, 0);                          /* Checksum placeholder */
    write_u16(tcp_hdr + 18, 0);                          /* Urgent pointer */

    /* Copy data */
    if (data_len > 0)
        memcpy(tcp_hdr + TCP_HEADER_LEN, data, (unsigned int)data_len);

    /* Compute TCP checksum */
    cksum = tcp_checksum(my_ip, conn->remote_ip, tcp_hdr, tcp_len);
    write_u16(tcp_hdr + 16, cksum);

    total_len = ETH_HEADER_LEN + IP_HEADER_LEN + tcp_len;
    sys_net_send(tx_buf, total_len);
}

static void tcp_send_rst(unsigned int remote_ip, int remote_port,
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

/* ---- HTTP ---- */

static const char *http_404 =
    "HTTP/1.0 404 Not Found\r\n"
    "Content-Type: text/html\r\n"
    "Content-Length: 48\r\n"
    "Connection: close\r\n"
    "\r\n"
    "<html><body><h1>404 Not Found</h1></body></html>";

static const char *http_405 =
    "HTTP/1.0 405 Method Not Allowed\r\n"
    "Content-Length: 0\r\n"
    "Connection: close\r\n"
    "\r\n";

static const char *content_type_html = "text/html";
static const char *content_type_text = "text/plain";
static const char *content_type_bin  = "application/octet-stream";

static const char *get_content_type(const char *path)
{
    int len;
    len = (int)strlen(path);

    if (len >= 5 && strcmp(path + len - 5, ".html") == 0)
        return content_type_html;
    if (len >= 4 && strcmp(path + len - 4, ".htm") == 0)
        return content_type_html;
    if (len >= 4 && strcmp(path + len - 4, ".txt") == 0)
        return content_type_text;
    return content_type_bin;
}

/* Simple itoa for Content-Length */
static int int_to_str(int val, char *buf)
{
    char tmp[12];
    int i;
    int len;

    if (val == 0)
    {
        buf[0] = '0';
        buf[1] = '\0';
        return 1;
    }

    i = 0;
    while (val > 0)
    {
        tmp[i++] = '0' + (val % 10);
        val = val / 10;
    }
    len = i;
    for (i = 0; i < len; i++)
        buf[i] = tmp[len - 1 - i];
    buf[len] = '\0';
    return len;
}

static void http_parse_request(struct tcp_conn *conn)
{
    char *req;
    int i;
    int path_start;
    int path_end;

    req = conn->request;

    /* Must start with "GET " */
    if (conn->request_len < 4 ||
        req[0] != 'G' || req[1] != 'E' || req[2] != 'T' || req[3] != ' ')
    {
        /* Not GET — send 405 */
        conn->http_state = HTTP_DONE;
        tcp_send(conn, TCP_ACK | TCP_PSH, (char *)http_405,
                 (int)strlen(http_405));
        conn->seq_local += (unsigned int)strlen(http_405);
        return;
    }

    /* Extract path */
    path_start = 4;
    path_end = path_start;
    while (path_end < conn->request_len && req[path_end] != ' ' &&
           req[path_end] != '\r' && req[path_end] != '\n')
        path_end++;

    /* Build full path: /www + request path */
    conn->path[0] = '/';
    conn->path[1] = 'w';
    conn->path[2] = 'w';
    conn->path[3] = 'w';
    i = 4;

    if (path_end - path_start == 1 && req[path_start] == '/')
    {
        /* Root → /www/index.html */
        memcpy(conn->path + 4, "/index.html", 11);
        i = 15;
    }
    else
    {
        int plen;
        plen = path_end - path_start;
        if (plen >= HTTP_PATH_SIZE - 5)
            plen = HTTP_PATH_SIZE - 5;
        memcpy(conn->path + 4, req + path_start, (unsigned int)plen);
        i = 4 + plen;
    }
    conn->path[i] = '\0';

    /* Open file */
    conn->response_fd = sys_open(conn->path, O_RDONLY);
    if (conn->response_fd < 0)
    {
        /* 404 */
        conn->http_state = HTTP_DONE;
        tcp_send(conn, TCP_ACK | TCP_PSH, (char *)http_404,
                 (int)strlen(http_404));
        conn->seq_local += (unsigned int)strlen(http_404);
        return;
    }

    /* Get file size via lseek */
    conn->response_size = sys_lseek(conn->response_fd, 0, SEEK_END);
    sys_lseek(conn->response_fd, 0, SEEK_SET);
    conn->response_sent = 0;
    conn->http_state = HTTP_SEND_HDR;
}

static void http_send_header(struct tcp_conn *conn)
{
    char hdr[256];
    int pos;
    const char *ct;
    char size_str[12];
    int slen;

    ct = get_content_type(conn->path);
    int_to_str(conn->response_size, size_str);
    slen = (int)strlen(size_str);

    /* Build response header */
    pos = 0;
    memcpy(hdr + pos, "HTTP/1.0 200 OK\r\n", 17); pos += 17;
    memcpy(hdr + pos, "Content-Type: ", 14); pos += 14;
    memcpy(hdr + pos, ct, strlen(ct)); pos += (int)strlen(ct);
    memcpy(hdr + pos, "\r\n", 2); pos += 2;
    memcpy(hdr + pos, "Content-Length: ", 16); pos += 16;
    memcpy(hdr + pos, size_str, (unsigned int)slen); pos += slen;
    memcpy(hdr + pos, "\r\n", 2); pos += 2;
    memcpy(hdr + pos, "Connection: close\r\n", 19); pos += 19;
    memcpy(hdr + pos, "\r\n", 2); pos += 2;

    tcp_send(conn, TCP_ACK | TCP_PSH, hdr, pos);
    conn->seq_local += (unsigned int)pos;
    conn->http_state = HTTP_SEND_BODY;
}

static void http_send_body_chunk(struct tcp_conn *conn)
{
    char chunk[1400];
    int to_send;
    int n;

    to_send = conn->response_size - conn->response_sent;
    if (to_send <= 0)
    {
        /* Done sending — close connection */
        tcp_send(conn, TCP_ACK | TCP_FIN, (char *)0, 0);
        conn->seq_local += 1; /* FIN consumes one seq */
        conn->state = STATE_FIN_WAIT;
        conn->http_state = HTTP_DONE;
        return;
    }

    if (to_send > 1400)
        to_send = 1400;

    n = sys_read(conn->response_fd, chunk, to_send);
    if (n <= 0)
    {
        /* Read error — close */
        tcp_send(conn, TCP_ACK | TCP_FIN, (char *)0, 0);
        conn->seq_local += 1;
        conn->state = STATE_FIN_WAIT;
        conn->http_state = HTTP_DONE;
        return;
    }

    tcp_send(conn, TCP_ACK | TCP_PSH, chunk, n);
    conn->seq_local += (unsigned int)n;
    conn->response_sent += n;
}

/* ---- TCP input handling ---- */

static void handle_tcp(const char *frame, int len, unsigned int src_ip)
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

    /* Find or create connection */
    conn = tcp_find_conn(src_ip, src_port);

    if (!conn)
    {
        /* New connection — must be SYN */
        if (!(flags & TCP_SYN))
        {
            tcp_send_rst(src_ip, src_port, dst_port, ack, seq + 1);
            return;
        }

        conn = tcp_alloc_conn();
        if (!conn)
        {
            /* No free slots */
            tcp_send_rst(src_ip, src_port, dst_port, 0, seq + 1);
            return;
        }

        conn->state = STATE_SYN_RCVD;
        conn->remote_ip = src_ip;
        conn->remote_port = src_port;
        conn->seq_remote = seq + 1;
        conn->seq_local = (unsigned int)sys_get_time_us(); /* Use time as ISN */
        conn->http_state = HTTP_RECV_REQ;
        conn->request_len = 0;
        conn->response_fd = -1;
        conn->last_activity = (unsigned int)sys_get_time_us();

        /* Send SYN+ACK */
        tcp_send(conn, TCP_SYN | TCP_ACK, (char *)0, 0);
        conn->seq_local += 1; /* SYN consumes one seq */
        return;
    }

    /* Update activity */
    conn->last_activity = (unsigned int)sys_get_time_us();

    /* Handle RST */
    if (flags & TCP_RST)
    {
        tcp_free_conn(conn);
        return;
    }

    switch (conn->state)
    {
    case STATE_SYN_RCVD:
        if (flags & TCP_ACK)
        {
            conn->state = STATE_ESTABLISHED;
        }
        break;

    case STATE_ESTABLISHED:
        /* Accept data */
        if (tcp_data_len > 0)
        {
            /* Check sequence matches expected */
            if (seq == conn->seq_remote)
            {
                /* Append to request buffer */
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

                /* ACK the data */
                tcp_send(conn, TCP_ACK, (char *)0, 0);

                /* Check if request is complete (has \r\n\r\n) */
                if (conn->http_state == HTTP_RECV_REQ &&
                    strstr(conn->request, "\r\n\r\n"))
                {
                    http_parse_request(conn);
                }
            }
        }

        /* Peer closing */
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
            /* Our FIN was acked */
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

/* ---- IP input ---- */

static void handle_ip(const char *frame, int len)
{
    unsigned int dst_ip;
    unsigned int src_ip;
    int protocol;
    int ip_hdr_len;

    if (len < ETH_HEADER_LEN + IP_HEADER_LEN) return;

    ip_hdr_len = (frame[ETH_HEADER_LEN] & 0x0F) * 4;
    if (ip_hdr_len < 20) return;

    src_ip = read_u32(frame + ETH_HEADER_LEN + 12);
    dst_ip = read_u32(frame + ETH_HEADER_LEN + 16);
    protocol = frame[ETH_HEADER_LEN + 9] & 0xFF;

    /* Only accept packets for our IP */
    if (dst_ip != my_ip) return;

    /* Learn sender MAC from IP packets too */
    arp_table_add(src_ip, frame + 6);

    switch (protocol)
    {
    case IP_PROTO_ICMP:
        handle_icmp(frame, len, src_ip);
        break;
    case IP_PROTO_TCP:
        handle_tcp(frame, len, src_ip);
        break;
    }
}

/* ---- Frame dispatch ---- */

static void handle_frame(const char *frame, int len)
{
    unsigned int ethertype;

    if (len < ETH_HEADER_LEN) return;

    ethertype = read_u16(frame + 12);

    switch (ethertype)
    {
    case ETHERTYPE_ARP:
        handle_arp(frame, len);
        break;
    case ETHERTYPE_IP:
        handle_ip(frame, len);
        break;
    }
}

/* ---- Connection maintenance ---- */

static void tcp_service_connections(void)
{
    int i;
    struct tcp_conn *conn;

    for (i = 0; i < MAX_CONNECTIONS; i++)
    {
        conn = &connections[i];
        if (conn->state != STATE_ESTABLISHED) continue;

        switch (conn->http_state)
        {
        case HTTP_SEND_HDR:
            http_send_header(conn);
            break;
        case HTTP_SEND_BODY:
            http_send_body_chunk(conn);
            break;
        }
    }
}

static void tcp_check_timeouts(void)
{
    int i;
    unsigned int now;
    struct tcp_conn *conn;

    now = (unsigned int)sys_get_time_us();

    for (i = 0; i < MAX_CONNECTIONS; i++)
    {
        conn = &connections[i];
        if (conn->state == STATE_FREE) continue;

        /* 30 second timeout */
        if ((now - conn->last_activity) > 30000000)
        {
            if (conn->state == STATE_ESTABLISHED)
            {
                tcp_send(conn, TCP_RST, (char *)0, 0);
            }
            tcp_free_conn(conn);
        }
    }
}

/* ---- Init ---- */

static void net_init_local(void)
{
    int i;

    /* sys_net_get_mac writes 6 sequential bytes to the buffer */
    syscall(SYS_NET_GET_MAC, (int)my_mac, 0, 0);

    my_ip = ((unsigned int)MY_IP_0 << 24) |
            ((unsigned int)MY_IP_1 << 16) |
            ((unsigned int)MY_IP_2 << 8) |
            ((unsigned int)MY_IP_3);

    my_gateway = ((unsigned int)MY_GATEWAY_0 << 24) |
                 ((unsigned int)MY_GATEWAY_1 << 16) |
                 ((unsigned int)MY_GATEWAY_2 << 8) |
                 ((unsigned int)MY_GATEWAY_3);

    /* Clear ARP table */
    for (i = 0; i < ARP_TABLE_SIZE; i++)
        arp_table[i].valid = 0;

    /* Clear connections */
    for (i = 0; i < MAX_CONNECTIONS; i++)
    {
        connections[i].state = STATE_FREE;
        connections[i].response_fd = -1;
    }
}

/* ---- Main ---- */

int main(int argc, char **argv)
{
    int len;
    int poll_count;

    net_init_local();

    sys_putstr("webserv: listening on 192.168.0.250:80\n");

    /* Send gratuitous ARP to announce ourselves */
    arp_send_request(my_ip);

    poll_count = 0;

    while (1)
    {
        len = sys_net_recv(frame_buf, MAX_FRAME_LEN);
        if (len > 0)
        {
            handle_frame(frame_buf, len);
        }

        /* Service HTTP response sends */
        tcp_service_connections();

        poll_count++;
        if (poll_count >= 10000)
        {
            poll_count = 0;
            tcp_check_timeouts();
            /* Re-announce ourselves periodically */
            arp_send_request(my_ip);
        }

        /* Yield to let other processes run */
        sys_yield();
    }

    return 0;
}
