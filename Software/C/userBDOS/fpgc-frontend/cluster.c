#include <string.h>
#include <syscall.h>
#include <fnp.h>
#include "net.h"
#include "tcp.h"
#include "cluster.h"

/* FNP message types — must match fnp.h */
#define FNP_TYPE_TETRIS_BOARD      0x52
#define FNP_TYPE_TETRIS_GA_STATUS  0x54
#define FNP_TYPE_CLUSTER_PARAMS    0x40
#define FNP_TYPE_CLUSTER_ASSIGN    0x41
#define FNP_TYPE_CLUSTER_RESULT    0x42

/* FNP header offsets (after 14-byte Ethernet header):
 * [14] Version  [15] Type  [16-17] Seq  [18] Flags  [19-20] Length  [21+] Data */
#define FNP_HDR_TYPE    15
#define FNP_HDR_SEQ     16
#define FNP_HDR_FLAGS   18
#define FNP_HDR_LEN     19
#define FNP_HDR_DATA    21

/* Mandelbrot worker MAC address (Device 4: 02:B4:B4:00:00:04) */
static int mbrot_worker_mac[6] = {0x02, 0xB4, 0xB4, 0x00, 0x00, 0x04};

/* FNP sequence counter and frame buffer for outgoing messages */
static int fnp_tx_seq;
static char fnp_frame_buf[FNP_FRAME_BUF_SIZE];

/* SSE client tracking */
#define MAX_SSE_CLIENTS 4

struct sse_client {
    int active;
    struct tcp_conn *conn;
    int type;
    int replay_idx;  /* >= 0: next history index to send; -1: done replaying */
};

static struct sse_client sse_clients[MAX_SSE_CLIENTS];

struct tetris_state tetris_state;
struct mbrot_state mbrot_state;

int hist_best[HIST_MAX];
int hist_avg[HIST_MAX];
int hist_mut[HIST_MAX];
int hist_count;

void cluster_init(void)
{
    memset(&tetris_state, 0, sizeof(tetris_state));
    memset(&mbrot_state, 0, sizeof(mbrot_state));
    memset(sse_clients, 0, sizeof(sse_clients));
    hist_count = 0;
    fnp_tx_seq = 0;
    fnp_init(); /* Initialize userlib FNP (reads MAC) */
}

/* Forward declaration */
#define HIST_BATCH 20
static int push_history_batch(struct tcp_conn *conn, int start);

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

            /* Defer history replay to the main loop (one batch per tick) */
            if (type == SSE_TETRIS && hist_count > 0)
                sse_clients[i].replay_idx = 0;
            else
                sse_clients[i].replay_idx = -1;

            return;
        }
    }
}

static void sse_send_event(struct tcp_conn *conn, const char *data, int len)
{
    /* Build "data: " + data + "\n\n" into a single TCP segment */
    char sse_buf[900];
    int total;

    memcpy(sse_buf, "data: ", 6);
    memcpy(sse_buf + 6, data, (unsigned int)len);
    sse_buf[6 + len] = '\n';
    sse_buf[6 + len + 1] = '\n';
    total = 6 + len + 2;

    tcp_send(conn, TCP_ACK | TCP_PSH, sse_buf, total);
    conn->seq_local += (unsigned int)total;
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
    int i, len, neg;
    if (val == 0) { buf[0] = '0'; buf[1] = '\0'; return 1; }
    neg = 0;
    if (val < 0) { neg = 1; val = 0 - val; }
    i = 0;
    while (val > 0) { tmp[i++] = '0' + (val % 10); val /= 10; }
    len = 0;
    if (neg) buf[len++] = '-';
    while (i > 0) buf[len++] = tmp[--i];
    buf[len] = '\0';
    return len;
}

static void push_tetris_sse(struct tcp_conn *conn)
{
    char buf[640];
    int pos;
    int i;

    pos = 0;
    memcpy(buf + pos, "{\"score\":", 9); pos += 9;
    pos += simple_itoa(tetris_state.score, buf + pos);
    memcpy(buf + pos, ",\"lines\":", 9); pos += 9;
    pos += simple_itoa(tetris_state.lines, buf + pos);
    memcpy(buf + pos, ",\"gen\":", 7); pos += 7;
    pos += simple_itoa(tetris_state.generation, buf + pos);
    memcpy(buf + pos, ",\"best\":", 8); pos += 8;
    pos += simple_itoa(tetris_state.alltime_best, buf + pos);
    memcpy(buf + pos, ",\"avg\":", 7); pos += 7;
    pos += simple_itoa(tetris_state.avg_score, buf + pos);
    memcpy(buf + pos, ",\"mut\":", 7); pos += 7;
    pos += simple_itoa(tetris_state.mutations, buf + pos);
    memcpy(buf + pos, ",\"player\":", 10); pos += 10;
    pos += simple_itoa(tetris_state.player, buf + pos);
    memcpy(buf + pos, ",\"pop\":", 7); pos += 7;
    pos += simple_itoa(tetris_state.pop_size, buf + pos);
    memcpy(buf + pos, ",\"genes\":[", 10); pos += 10;
    for (i = 0; i < 6; i++)
    {
        if (i > 0) buf[pos++] = ',';
        pos += simple_itoa(tetris_state.genes[i], buf + pos);
    }
    buf[pos++] = ']';
    memcpy(buf + pos, ",\"board\":\"", 10); pos += 10;
    for (i = 0; i < 200; i++)
    {
        hex_byte(tetris_state.board[i], buf + pos);
        pos += 2;
    }
    buf[pos++] = '"';
    buf[pos++] = '}';

    sse_send_event(conn, buf, pos);
}

static void push_mbrot_sse(struct tcp_conn *conn)
{
    char buf[800];
    int pos;
    int offset;
    int pixel_count;
    int received_pixels;
    int total_pixels;
    int i;

    if (!mbrot_state.rendering)
        return;

    /* How many pixels have been received so far? */
    received_pixels = mbrot_state.chunks_received * MBROT_CHUNK_SIZE;
    if (received_pixels > MBROT_PIXELS)
        received_pixels = MBROT_PIXELS;

    offset = mbrot_state.last_sent_offset;
    if (offset >= received_pixels)
        return;  /* no new data to stream */

    pixel_count = received_pixels - offset;
    if (pixel_count > 350)
        pixel_count = 350; /* 350 * 2 = 700 hex chars fits in buf */

    total_pixels = MBROT_PIXELS;

    /* Build JSON: {"type":"chunk","idx":N,"total":N,"off":N,"len":N,"pixels":"hex..."} */
    pos = 0;
    memcpy(buf + pos, "{\"type\":\"chunk\",\"idx\":", 22); pos += 22;
    pos += simple_itoa(offset / 350, buf + pos);
    memcpy(buf + pos, ",\"total\":", 9); pos += 9;
    pos += simple_itoa((total_pixels + 349) / 350, buf + pos);
    memcpy(buf + pos, ",\"off\":", 7); pos += 7;
    pos += simple_itoa(offset, buf + pos);
    memcpy(buf + pos, ",\"len\":", 7); pos += 7;
    pos += simple_itoa(pixel_count, buf + pos);
    memcpy(buf + pos, ",\"pixels\":\"", 11); pos += 11;

    for (i = 0; i < pixel_count; i++)
    {
        hex_byte(mbrot_state.pixels[offset + i], buf + pos);
        pos += 2;
    }

    buf[pos++] = '"';
    buf[pos++] = '}';

    sse_send_event(conn, buf, pos);
    mbrot_state.last_sent_offset = offset + pixel_count;

    /* Send done event when all pixels received and streamed */
    if (mbrot_state.last_sent_offset >= MBROT_PIXELS
        && mbrot_state.chunks_received >= mbrot_state.total_chunks)
    {
        char done[32];
        int dpos;
        dpos = 0;
        memcpy(done, "{\"type\":\"done\"}", 15); dpos = 15;
        sse_send_event(conn, done, dpos);
        mbrot_state.rendering = 0;
    }
}

/* Send a batch of history entries as a single SSE event.
 * Format: {"type":"hist","b":[...],"a":[...],"m":[...]}
 * Returns number of entries sent. */

static int push_history_batch(struct tcp_conn *conn, int start)
{
    char buf[800];
    int pos;
    int end;
    int i;

    end = start + HIST_BATCH;
    if (end > hist_count) end = hist_count;
    if (start >= end) return 0;

    pos = 0;
    memcpy(buf + pos, "{\"type\":\"hist\",\"b\":[", 20); pos += 20;
    for (i = start; i < end; i++)
    {
        if (i > start) buf[pos++] = ',';
        pos += simple_itoa(hist_best[i], buf + pos);
    }
    memcpy(buf + pos, "],\"a\":[", 7); pos += 7;
    for (i = start; i < end; i++)
    {
        if (i > start) buf[pos++] = ',';
        pos += simple_itoa(hist_avg[i], buf + pos);
    }
    memcpy(buf + pos, "],\"m\":[", 7); pos += 7;
    for (i = start; i < end; i++)
    {
        if (i > start) buf[pos++] = ',';
        pos += simple_itoa(hist_mut[i], buf + pos);
    }
    memcpy(buf + pos, "]}", 2); pos += 2;

    sse_send_event(conn, buf, pos);
    return end - start;
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

        /* Deferred history replay: one batch per main-loop tick */
        if (sse_clients[i].replay_idx >= 0)
        {
            int sent;
            sent = push_history_batch(sse_clients[i].conn,
                                      sse_clients[i].replay_idx);
            if (sent > 0)
                sse_clients[i].replay_idx += sent;
            if (sse_clients[i].replay_idx >= hist_count)
                sse_clients[i].replay_idx = -1;  /* replay done */
            continue;  /* skip live updates until replay finishes */
        }

        if (sse_clients[i].type == SSE_TETRIS && tetris_state.updated)
            push_tetris_sse(sse_clients[i].conn);

        if (sse_clients[i].type == SSE_MANDELBROT && mbrot_state.rendering)
            push_mbrot_sse(sse_clients[i].conn);
    }

    tetris_state.updated = 0;
    mbrot_state.updated = 0;
}

/* Handle incoming FNP cluster messages */
void cluster_handle(const char *frame, int len)
{
    int msg_type;
    int rx_flags;
    int payload_len;
    const char *payload;

    if (len < FNP_HDR_DATA) return;

    msg_type = (int)(unsigned char)frame[FNP_HDR_TYPE];
    rx_flags = (int)(unsigned char)frame[FNP_HDR_FLAGS];
    payload_len = (int)read_u16(frame + FNP_HDR_LEN);
    payload = frame + FNP_HDR_DATA;

    if (FNP_HDR_DATA + payload_len > len)
        payload_len = len - FNP_HDR_DATA;

    /* Send ACK if the sender requested one */
    if (rx_flags & 0x02) /* FNP_FLAG_REQUIRES_ACK */
    {
        int src_mac[6];
        char ack_data[2];
        int seq_hi;
        int seq_lo;
        int i;

        for (i = 0; i < 6; i++)
            src_mac[i] = (int)(unsigned char)frame[6 + i];

        seq_hi = (int)(unsigned char)frame[FNP_HDR_SEQ];
        seq_lo = (int)(unsigned char)frame[FNP_HDR_SEQ + 1];
        ack_data[0] = (char)seq_hi;
        ack_data[1] = (char)seq_lo;

        fnp_send(src_mac, 0x01 /* FNP_TYPE_ACK */, fnp_tx_seq, 0,
                 ack_data, 2, fnp_frame_buf);
        fnp_tx_seq++;
    }

    switch (msg_type)
    {
    case FNP_TYPE_TETRIS_BOARD:
        if (payload_len >= 212)
        {
            memcpy(tetris_state.board, payload, 200);
            tetris_state.score = (int)read_u32(payload + 200);
            tetris_state.lines = (int)read_u32(payload + 204);
            tetris_state.pieces = (int)read_u32(payload + 208);
            if (payload_len >= 252)
            {
                int gi;
                tetris_state.generation = (int)read_u32(payload + 212);
                tetris_state.player = (int)read_u32(payload + 216);
                tetris_state.pop_size = (int)read_u32(payload + 220);
                tetris_state.alltime_best = (int)read_u32(payload + 224);
                for (gi = 0; gi < 6; gi++)
                    tetris_state.genes[gi] = (int)read_u32(payload + 228 + gi * 4);
            }
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
            if (payload_len >= 52)
            {
                int gi;
                for (gi = 0; gi < 6; gi++)
                    tetris_state.genes[gi] = (int)read_u32(payload + 28 + gi * 4);
            }
            tetris_state.updated = 1;

            /* Record history */
            if (hist_count < HIST_MAX)
            {
                hist_best[hist_count] = tetris_state.alltime_best;
                hist_avg[hist_count] = tetris_state.avg_score;
                hist_mut[hist_count] = tetris_state.mutations;
                hist_count++;
            }
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
            offset = chunk_idx * MBROT_CHUNK_SIZE;

            if (offset + pixel_count <= MBROT_PIXELS)
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

/* Send an FNP frame using the userlib fnp_send() function.
 * Uses the same proven code path as mbroth.c. */
static void cluster_fnp_send(int msg_type, const char *data, int data_len)
{
    fnp_send(mbrot_worker_mac, msg_type, fnp_tx_seq, 0,
             (char *)data, data_len, fnp_frame_buf);
    fnp_tx_seq++;
}

/* Parse a URL-encoded form field value.
 * Searches for "name=" in body, copies value until '&' or end.
 * Returns number of chars written (0 if not found). */
static int parse_form_field(const char *body, const char *name,
                            char *out, int max_out)
{
    const char *p;
    const char *bp;
    int nlen;
    int i;

    if (body == (char *)0)
        return 0;

    nlen = (int)strlen(name);
    bp = body;

    while (*bp)
    {
        /* Check if name matches at this position */
        p = bp;
        i = 0;
        while (i < nlen && *p == name[i])
        {
            p++;
            i++;
        }
        if (i == nlen && *p == '=')
        {
            p++; /* skip '=' */
            i = 0;
            while (*p && *p != '&' && i < max_out - 1)
            {
                out[i++] = *p++;
            }
            out[i] = '\0';
            return i;
        }
        /* Skip to next '&' */
        while (*bp && *bp != '&')
            bp++;
        if (*bp == '&')
            bp++;
    }
    return 0;
}

/* Parse a decimal/hex integer string.
 * Supports optional leading '-' and "0x" prefix. */
static int parse_int_str(const char *s)
{
    int neg;
    int val;

    neg = 0;
    val = 0;

    if (*s == '-')
    {
        neg = 1;
        s++;
    }
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
    {
        s += 2;
        while (*s)
        {
            int d;
            if (*s >= '0' && *s <= '9') d = *s - '0';
            else if (*s >= 'a' && *s <= 'f') d = *s - 'a' + 10;
            else if (*s >= 'A' && *s <= 'F') d = *s - 'A' + 10;
            else break;
            val = (val << 4) | d;
            s++;
        }
    }
    else
    {
        while (*s >= '0' && *s <= '9')
        {
            val = val * 10 + (*s - '0');
            s++;
        }
    }
    return neg ? (0 - val) : val;
}

/* Handle mandelbrot render request from HTTP POST.
 * Body format: center_re_hi=N&center_re_lo=N&center_im_hi=N&center_im_lo=N
 *              &scale_hi=N&scale_lo=N&max_iter=N
 * Values are integers (decimal or 0x hex), representing Q32.32 fixed-point
 * for center_re, center_im, scale. */
void cluster_handle_render_request(struct tcp_conn *conn, const char *body)
{
    char val_buf[20];
    unsigned int center_re_hi;
    unsigned int center_re_lo;
    unsigned int center_im_hi;
    unsigned int center_im_lo;
    unsigned int scale_hi;
    unsigned int scale_lo;
    unsigned int max_iter;
    const char *resp;
    int resp_len;

    sys_putstr("[MBROT] render request\n");

    /* Parse form fields — use defaults matching mbroth.c reset_view */
    center_re_hi = 0xFFFFFFFF; /* -1 */
    center_re_lo = 0x41A1BEB0;
    center_im_hi = 0x00000000;
    center_im_lo = 0x21C01C90;
    scale_hi     = 0x00000003;
    scale_lo     = 0x80000000;
    max_iter     = 100;

    if (body != (char *)0)
    {
        if (parse_form_field(body, "center_re_hi", val_buf, 20))
            center_re_hi = (unsigned int)parse_int_str(val_buf);
        if (parse_form_field(body, "center_re_lo", val_buf, 20))
            center_re_lo = (unsigned int)parse_int_str(val_buf);
        if (parse_form_field(body, "center_im_hi", val_buf, 20))
            center_im_hi = (unsigned int)parse_int_str(val_buf);
        if (parse_form_field(body, "center_im_lo", val_buf, 20))
            center_im_lo = (unsigned int)parse_int_str(val_buf);
        if (parse_form_field(body, "scale_hi", val_buf, 20))
            scale_hi = (unsigned int)parse_int_str(val_buf);
        if (parse_form_field(body, "scale_lo", val_buf, 20))
            scale_lo = (unsigned int)parse_int_str(val_buf);
        if (parse_form_field(body, "max_iter", val_buf, 20))
            max_iter = (unsigned int)parse_int_str(val_buf);
    }

    /* Reset mandelbrot state for new render */
    memset(mbrot_state.pixels, 0, MBROT_PIXELS);
    mbrot_state.rendering = 1;
    mbrot_state.width = MBROT_WIDTH;
    mbrot_state.height = MBROT_HEIGHT;
    mbrot_state.chunks_received = 0;
    mbrot_state.total_chunks = MBROT_TOTAL_CHUNKS;
    mbrot_state.last_sent_offset = 0;
    mbrot_state.updated = 1;

    /* Store params for deferred send from main loop */
    mbrot_state.params[0] = center_re_hi;
    mbrot_state.params[1] = center_re_lo;
    mbrot_state.params[2] = center_im_hi;
    mbrot_state.params[3] = center_im_lo;
    mbrot_state.params[4] = scale_hi;
    mbrot_state.params[5] = scale_lo;
    mbrot_state.params[6] = max_iter;
    mbrot_state.send_pending = 1;

    /* Respond with 200 OK */
    resp =
        "HTTP/1.0 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: 15\r\n"
        "Connection: close\r\n"
        "\r\n"
        "{\"status\":\"ok\"}";
    resp_len = (int)strlen(resp);
    tcp_send(conn, TCP_ACK | TCP_PSH, resp, resp_len);
    conn->seq_local += (unsigned int)resp_len;
    conn->http_state = HTTP_DONE;
}

/* Called from main event loop — sends deferred FNP messages */
void cluster_poll(void)
{
    char params[28];
    char assign[12];

    if (!mbrot_state.send_pending)
        return;

    mbrot_state.send_pending = 0;

    /* Build CLUSTER_PARAMS payload (28 bytes, matching mbroth.c) */
    write_u32(params + 0,  mbrot_state.params[0]);
    write_u32(params + 4,  mbrot_state.params[1]);
    write_u32(params + 8,  mbrot_state.params[2]);
    write_u32(params + 12, mbrot_state.params[3]);
    write_u32(params + 16, mbrot_state.params[4]);
    write_u32(params + 20, mbrot_state.params[5]);
    write_u32(params + 24, mbrot_state.params[6]);

    /* Send CLUSTER_PARAMS to mandelbrot worker */
    cluster_fnp_send(FNP_TYPE_CLUSTER_PARAMS, params, 28);

    /* Build CLUSTER_ASSIGN payload (12 bytes): worker, rows 0–240 */
    write_u32(assign + 0, (unsigned int)(mbrot_worker_mac[5] - 0x02)); /* worker_id */
    write_u32(assign + 4, 0);            /* y_start */
    write_u32(assign + 8, MBROT_HEIGHT); /* y_end */

    /* Send CLUSTER_ASSIGN to mandelbrot worker */
    cluster_fnp_send(FNP_TYPE_CLUSTER_ASSIGN, assign, 12);
}
