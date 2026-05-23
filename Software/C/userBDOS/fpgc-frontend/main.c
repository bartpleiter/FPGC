#include <string.h>
#include <syscall.h>
#include "net.h"
#include "arp.h"
#include "ip.h"
#include "tcp.h"
#include "http.h"
#include "cluster.h"

static char frame_buf[MAX_FRAME_LEN];

static void net_init(void)
{
    /* Get our MAC address (writes 6 bytes to buffer) */
    syscall(SYS_NET_GET_MAC, (int)my_mac, 0, 0);

    /* Build IP addresses as 32-bit big-endian */
    my_ip = ((unsigned int)MY_IP_0 << 24) |
            ((unsigned int)MY_IP_1 << 16) |
            ((unsigned int)MY_IP_2 << 8) |
            (unsigned int)MY_IP_3;

    my_gateway = ((unsigned int)MY_GATEWAY_0 << 24) |
                 ((unsigned int)MY_GATEWAY_1 << 16) |
                 ((unsigned int)MY_GATEWAY_2 << 8) |
                 (unsigned int)MY_GATEWAY_3;
}

int main(void)
{
    int len;
    int poll_count;
    unsigned int ethertype;

    net_init();
    cluster_init();

    sys_putstr("fpgc-frontend: listening on 192.168.0.250:80\n");

    /* Gratuitous ARP to announce ourselves */
    arp_send_request(my_ip);

    poll_count = 0;

    while (1)
    {
        /* 1. Poll for incoming Ethernet frames */
        len = sys_net_recv(frame_buf, MAX_FRAME_LEN);
        if (len > 0)
        {
            ethertype = read_u16(frame_buf + 12);

            if (ethertype == ETHERTYPE_ARP)
                arp_handle(frame_buf, len);
            else if (ethertype == ETHERTYPE_IP)
                ip_handle(frame_buf, len);
            else if (ethertype == ETHERTYPE_FNP)
                cluster_handle(frame_buf, len);
        }

        /* 2. Service TCP connections (send pending data) */
        tcp_service();

        /* 3. Push SSE events if cluster state changed */
        cluster_push_sse();

        /* 4. Periodic maintenance */
        poll_count++;
        if (poll_count >= 10000)
        {
            poll_count = 0;
            tcp_check_timeouts();
            arp_send_request(my_ip);
        }

        /* 5. Yield to other processes */
        sys_yield();
    }

    return 0;
}
