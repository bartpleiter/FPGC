#ifndef NET_H
#define NET_H

/* Network configuration */
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

/* Protocol constants */
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
#define ETHERTYPE_FNP   0xB4B4

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
#define STATE_SYN_RCVD  1
#define STATE_ESTABLISHED 2
#define STATE_FIN_WAIT  3

/* HTTP states */
#define HTTP_RECV_REQ   0
#define HTTP_SEND_HDR   1
#define HTTP_SEND_BODY  2
#define HTTP_DONE       3
#define HTTP_SSE        4

/* Connection limits */
#define ARP_TABLE_SIZE  8
#define MAX_CONNECTIONS 8
#define HTTP_REQ_SIZE   1024
#define HTTP_PATH_SIZE  128

/* Shared frame buffers */
extern char my_mac[6];
extern char tx_buf[MAX_FRAME_LEN];
extern unsigned int my_ip;
extern unsigned int my_gateway;

/* Structs */
struct arp_entry {
    unsigned int ip;
    char mac[6];
    int valid;
};

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

/* Utility functions */
unsigned int read_u16(const char *buf);
unsigned int read_u32(const char *buf);
void write_u16(char *buf, unsigned int val);
void write_u32(char *buf, unsigned int val);
unsigned int ip_checksum(const char *buf, int len);
unsigned int tcp_checksum(unsigned int src_ip, unsigned int dst_ip,
                          const char *tcp_hdr, int tcp_len);
void eth_build(char *frame, const char *dst_mac, unsigned int ethertype);

#endif
