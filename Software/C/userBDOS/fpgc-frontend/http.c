#include <string.h>
#include <syscall.h>
#include "net.h"
#include "tcp.h"
#include "http.h"
#include "cluster.h"

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
static const char *content_type_css  = "text/css";
static const char *content_type_js   = "application/javascript";
static const char *content_type_json = "application/json";
static const char *content_type_text = "text/plain";
static const char *content_type_sse  = "text/event-stream";
static const char *content_type_bin  = "application/octet-stream";

static const char *get_content_type(const char *path)
{
    int len;
    len = (int)strlen(path);

    if (len >= 5 && strcmp(path + len - 5, ".html") == 0)
        return content_type_html;
    if (len >= 4 && strcmp(path + len - 4, ".htm") == 0)
        return content_type_html;
    if (len >= 4 && strcmp(path + len - 4, ".css") == 0)
        return content_type_css;
    if (len >= 3 && strcmp(path + len - 3, ".js") == 0)
        return content_type_js;
    if (len >= 5 && strcmp(path + len - 5, ".json") == 0)
        return content_type_json;
    if (len >= 4 && strcmp(path + len - 4, ".txt") == 0)
        return content_type_text;
    return content_type_bin;
}

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

/* Check if path matches a prefix */
static int path_starts_with(const char *path, const char *prefix)
{
    while (*prefix)
    {
        if (*path != *prefix) return 0;
        path++;
        prefix++;
    }
    return 1;
}

/* SSE response headers */
static void http_start_sse(struct tcp_conn *conn)
{
    const char *hdr =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/event-stream\r\n"
        "Cache-Control: no-cache\r\n"
        "Connection: keep-alive\r\n"
        "\r\n";
    int hdr_len;

    hdr_len = (int)strlen(hdr);
    tcp_send(conn, TCP_ACK | TCP_PSH, hdr, hdr_len);
    conn->seq_local += (unsigned int)hdr_len;
    conn->http_state = HTTP_SSE;
}

/* Parse request and dispatch */
void http_handle_request(struct tcp_conn *conn)
{
    char *req;
    int path_start;
    int path_end;
    int plen;
    int i;

    req = conn->request;

    /* Extract method */
    if (conn->request_len >= 4 &&
        req[0] == 'G' && req[1] == 'E' && req[2] == 'T' && req[3] == ' ')
    {
        /* GET request */
        path_start = 4;
    }
    else if (conn->request_len >= 5 &&
             req[0] == 'P' && req[1] == 'O' && req[2] == 'S' &&
             req[3] == 'T' && req[4] == ' ')
    {
        /* POST request */
        path_start = 5;
    }
    else
    {
        conn->http_state = HTTP_DONE;
        tcp_send(conn, TCP_ACK | TCP_PSH, (char *)http_405,
                 (int)strlen(http_405));
        conn->seq_local += (unsigned int)strlen(http_405);
        return;
    }

    /* Extract path */
    path_end = path_start;
    while (path_end < conn->request_len && req[path_end] != ' ' &&
           req[path_end] != '\r' && req[path_end] != '\n')
        path_end++;

    plen = path_end - path_start;
    if (plen >= HTTP_PATH_SIZE - 1)
        plen = HTTP_PATH_SIZE - 2;
    memcpy(conn->path, req + path_start, (unsigned int)plen);
    conn->path[plen] = '\0';

    /* API endpoints */
    if (path_starts_with(conn->path, "/api/tetris") && req[0] == 'G')
    {
        http_start_sse(conn);
        cluster_register_sse(conn, SSE_TETRIS);
        return;
    }
    if (path_starts_with(conn->path, "/api/mbrot") && req[0] == 'G')
    {
        http_start_sse(conn);
        cluster_register_sse(conn, SSE_MANDELBROT);
        return;
    }
    if (path_starts_with(conn->path, "/api/mbrot") && req[0] == 'P')
    {
        /* POST — find body after \r\n\r\n */
        char *body;
        body = strstr(conn->request, "\r\n\r\n");
        if (body) body += 4;
        cluster_handle_render_request(conn, body);
        return;
    }

    /* Static file serving: map to /www/ on filesystem */
    {
        char fpath[HTTP_PATH_SIZE];
        fpath[0] = '/';
        fpath[1] = 'w';
        fpath[2] = 'w';
        fpath[3] = 'w';
        i = 4;

        if (plen == 1 && conn->path[0] == '/')
        {
            memcpy(fpath + 4, "/index.html", 11);
            i = 15;
        }
        else
        {
            if (plen >= HTTP_PATH_SIZE - 5)
                plen = HTTP_PATH_SIZE - 5;
            memcpy(fpath + 4, conn->path, (unsigned int)plen);
            i = 4 + plen;
        }
        fpath[i] = '\0';

        /* Copy fpath to conn->path for content-type lookup */
        memcpy(conn->path, fpath, (unsigned int)(i + 1));

        conn->response_fd = sys_open(fpath, O_RDONLY);
        if (conn->response_fd < 0)
        {
            conn->http_state = HTTP_DONE;
            tcp_send(conn, TCP_ACK | TCP_PSH, (char *)http_404,
                     (int)strlen(http_404));
            conn->seq_local += (unsigned int)strlen(http_404);
            return;
        }

        conn->response_size = sys_lseek(conn->response_fd, 0, SEEK_END);
        sys_lseek(conn->response_fd, 0, SEEK_SET);
        conn->response_sent = 0;
        conn->http_state = HTTP_SEND_HDR;
    }
}

void http_send_header(struct tcp_conn *conn)
{
    char hdr[256];
    int pos;
    const char *ct;
    char size_str[12];
    int slen;

    ct = get_content_type(conn->path);
    int_to_str(conn->response_size, size_str);
    slen = (int)strlen(size_str);

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

void http_send_body_chunk(struct tcp_conn *conn)
{
    char chunk[1400];
    int to_send;
    int n;

    to_send = conn->response_size - conn->response_sent;
    if (to_send <= 0)
    {
        tcp_send(conn, TCP_ACK | TCP_FIN, (char *)0, 0);
        conn->seq_local += 1;
        conn->state = STATE_FIN_WAIT;
        conn->http_state = HTTP_DONE;
        return;
    }

    if (to_send > 1400)
        to_send = 1400;

    n = sys_read(conn->response_fd, chunk, to_send);
    if (n <= 0)
    {
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
