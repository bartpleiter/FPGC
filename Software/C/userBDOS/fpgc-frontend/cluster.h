#ifndef CLUSTER_H
#define CLUSTER_H

#include "net.h"

/* SSE client types */
#define SSE_TETRIS      0
#define SSE_MANDELBROT  1

/* Cluster state */
struct tetris_state {
    int active;
    int status;
    int score;
    int lines;
    int pieces;
    unsigned char board[200];
    int generation;
    int best_score;
    int avg_score;
    int mutations;
    int alltime_best;
    int updated;
};

struct mbrot_state {
    int rendering;
    int width;
    int height;
    int chunks_received;
    int total_chunks;
    unsigned char pixels[320 * 240];
    int updated;
};

extern struct tetris_state tetris_state;
extern struct mbrot_state mbrot_state;

void cluster_init(void);
void cluster_handle(const char *frame, int len);
void cluster_register_sse(struct tcp_conn *conn, int type);
void cluster_push_sse(void);
void cluster_handle_render_request(struct tcp_conn *conn, const char *body);

#endif
