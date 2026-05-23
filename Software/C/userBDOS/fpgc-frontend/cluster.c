#include <string.h>
#include <syscall.h>
#include "net.h"
#include "tcp.h"
#include "cluster.h"

/* FNP message types (cluster-specific) */
#define FNP_TYPE_TETRIS_BOARD      0x44
#define FNP_TYPE_TETRIS_GA_STATUS  0x42
#define FNP_TYPE_CLUSTER_RESULT    0x34

/* FNP header offsets (after 14-byte Ethernet header) */
#define FNP_HDR_TYPE    14
#define FNP_HDR_SEQ     15
#define FNP_HDR_FLAGS   16
#define FNP_HDR_LEN     17
#define FNP_HDR_DATA    21

/* SSE client tracking */
#define MAX_SSE_CLIENTS 4

struct sse_client {
    int active;
    struct tcp_conn *conn;
    int type;
};

static struct sse_client sse_clients[MAX_SSE_CLIENTS];

struct tetris_state tetris_state;
struct mbrot_state mbrot_state;

void cluster_init(void)
{
    memset(&tetris_state, 0, sizeof(tetris_state));
    memset(&mbrot_state, 0, sizeof(mbrot_state));
    memset(sse_clients, 0, sizeof(sse_clients));
}

void cluster_register_sse(struct tcp_conn *conn, int type)
{
    int i;
    for (i = 0; i < MAX_SSE_CLIENTS; i++)
    {
        if (!sse_clients[i].active)
        {
            sse_clients[i].active = 1;
            sse_clients[i].conn = conn;
            sse_clients[i].type = type;
            return;
        }
    }
}

static void sse_send_event(struct tcp_conn *conn, const char *data, int len)
{
    /* Send "data: " prefix + data + "\n\n" */
    char prefix[7];
    char suffix[3];

    prefix[0] = 'd'; prefix[1] = 'a'; prefix[2] = 't';
    prefix[3] = 'a'; prefix[4] = ':'; prefix[5] = ' ';
    prefix[6] = '\0';
    suffix[0] = '\n'; suffix[1] = '\n'; suffix[2] = '\0';

    tcp_send(conn, TCP_ACK | TCP_PSH, prefix, 6);
    conn->seq_local += 6;
    tcp_send(conn, TCP_ACK | TCP_PSH, data, len);
    conn->seq_local += (unsigned int)len;
    tcp_send(conn, TCP_ACK | TCP_PSH, suffix, 2);
    conn->seq_local += 2;
}

static void hex_byte(unsigned char b, char *out)
{
    static const char hex[] = "0123456789abcdef";
    out[0] = hex[(b >> 4) & 0x0F];
    out[1] = hex[b & 0x0F];
}

static int simple_itoa(int val, char *buf)
{
    char tmp[12];
    int i, len;
    if (val == 0) { buf[0] = '0'; buf[1] = '\0'; return 1; }
    i = 0;
    while (val > 0) { tmp[i++] = '0' + (val % 10); val /= 10; }
    len = i;
    for (i = 0; i < len; i++) buf[i] = tmp[len - 1 - i];
    buf[len] = '\0';
    return len;
}

static void push_tetris_sse(struct tcp_conn *conn)
{
    /* JSON: {"type":"board","score":N,"lines":N,"gen":N,"best":N} */
    char buf[128];
    int pos;
    char numstr[12];

    pos = 0;
    memcpy(buf + pos, "{\"type\":\"board\",\"score\":", 23); pos += 23;
    pos += simple_itoa(tetris_state.score, buf + pos);
    memcpy(buf + pos, ",\"lines\":", 9); pos += 9;
    pos += simple_itoa(tetris_state.lines, buf + pos);
    memcpy(buf + pos, ",\"gen\":", 7); pos += 7;
    pos += simple_itoa(tetris_state.generation, buf + pos);
    memcpy(buf + pos, ",\"best\":", 8); pos += 8;
    pos += simple_itoa(tetris_state.alltime_best, buf + pos);
    buf[pos++] = '}';

    sse_send_event(conn, buf, pos);
}

static void push_mbrot_sse(struct tcp_conn *conn)
{
    /* JSON: {"type":"progress","chunks":N,"total":N} */
    char buf[64];
    int pos;

    pos = 0;
    memcpy(buf + pos, "{\"type\":\"progress\",\"chunks\":", 27); pos += 27;
    pos += simple_itoa(mbrot_state.chunks_received, buf + pos);
    memcpy(buf + pos, ",\"total\":", 9); pos += 9;
    pos += simple_itoa(mbrot_state.total_chunks, buf + pos);
    buf[pos++] = '}';

    sse_send_event(conn, buf, pos);
}

void cluster_push_sse(void)
{
    int i;

    for (i = 0; i < MAX_SSE_CLIENTS; i++)
    {
        if (!sse_clients[i].active) continue;

        /* Check if connection is still alive */
        if (sse_clients[i].conn->state != STATE_ESTABLISHED)
        {
            sse_clients[i].active = 0;
            continue;
        }

        if (sse_clients[i].type == SSE_TETRIS && tetris_state.updated)
            push_tetris_sse(sse_clients[i].conn);

        if (sse_clients[i].type == SSE_MANDELBROT && mbrot_state.updated)
            push_mbrot_sse(sse_clients[i].conn);
    }

    tetris_state.updated = 0;
    mbrot_state.updated = 0;
}

/* Handle incoming FNP cluster messages */
void cluster_handle(const char *frame, int len)
{
    int msg_type;
    int payload_len;
    const char *payload;

    if (len < FNP_HDR_DATA) return;

    msg_type = (int)(unsigned char)frame[FNP_HDR_TYPE];
    payload_len = (int)read_u16(frame + FNP_HDR_LEN);
    payload = frame + FNP_HDR_DATA;

    if (FNP_HDR_DATA + payload_len > len)
        payload_len = len - FNP_HDR_DATA;

    switch (msg_type)
    {
    case FNP_TYPE_TETRIS_BOARD:
        if (payload_len >= 212)
        {
            memcpy(tetris_state.board, payload, 200);
            tetris_state.score = (int)read_u32(payload + 200);
            tetris_state.lines = (int)read_u32(payload + 204);
            tetris_state.pieces = (int)read_u32(payload + 208);
            tetris_state.active = 1;
            tetris_state.updated = 1;
        }
        break;

    case FNP_TYPE_TETRIS_GA_STATUS:
        if (payload_len >= 28)
        {
            tetris_state.generation = (int)read_u32(payload + 0);
            tetris_state.best_score = (int)read_u32(payload + 8);
            tetris_state.alltime_best = (int)read_u32(payload + 12);
            tetris_state.avg_score = (int)read_u32(payload + 16);
            tetris_state.mutations = (int)read_u32(payload + 20);
            tetris_state.updated = 1;
        }
        break;

    case FNP_TYPE_CLUSTER_RESULT:
        if (payload_len >= 4)
        {
            int chunk_idx;
            int pixel_count;
            int offset;

            chunk_idx = (int)read_u32(payload);
            pixel_count = payload_len - 4;
            offset = chunk_idx * 1020;

            if (offset + pixel_count <= 320 * 240)
            {
                memcpy(mbrot_state.pixels + offset,
                       payload + 4, (unsigned int)pixel_count);
                mbrot_state.chunks_received++;
                mbrot_state.updated = 1;
            }
        }
        break;
    }
}

/* Handle mandelbrot render request from HTTP POST */
void cluster_handle_render_request(struct tcp_conn *conn, const char *body)
{
    /* TODO Phase 3: parse body, send FNP_CLUSTER_PARAMS + ASSIGN
       to the mandelbrot node. For now, return 200 OK stub. */
    const char *resp =
        "HTTP/1.0 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: 15\r\n"
        "Connection: close\r\n"
        "\r\n"
        "{\"status\":\"ok\"}";
    int resp_len;

    (void)body; /* Not yet used */
    resp_len = (int)strlen(resp);
    tcp_send(conn, TCP_ACK | TCP_PSH, resp, resp_len);
    conn->seq_local += (unsigned int)resp_len;
    conn->http_state = HTTP_DONE;

    /* Reset mandelbrot state for new render */
    mbrot_state.rendering = 1;
    mbrot_state.chunks_received = 0;
    mbrot_state.total_chunks = 76;  /* 320×240 / 1020 ≈ 76 */
    mbrot_state.updated = 0;
}
