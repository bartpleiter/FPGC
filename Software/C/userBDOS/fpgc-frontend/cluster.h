#ifndef CLUSTER_H
#define CLUSTER_H

#include "net.h"

/* SSE client types */
#define SSE_TETRIS      0
#define SSE_MANDELBROT  1

/* Mandelbrot render dimensions */
#define MBROT_WIDTH   320
#define MBROT_HEIGHT  240
#define MBROT_PIXELS  (MBROT_WIDTH * MBROT_HEIGHT)
#define MBROT_CHUNK_SIZE 1020
#define MBROT_TOTAL_CHUNKS ((MBROT_PIXELS + MBROT_CHUNK_SIZE - 1) / MBROT_CHUNK_SIZE)

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
    int genes[6];  /* Q16.16 best genes */
    int player;    /* 1-based current chromosome index */
    int pop_size;  /* total chromosomes per generation */
    int updated;
};

struct mbrot_state {
    int rendering;
    int width;
    int height;
    int chunks_received;
    int total_chunks;
    int last_sent_offset; /* next pixel offset to stream via SSE */
    int send_pending;    /* 1 = need to send PARAMS+ASSIGN to worker */
    unsigned int params[7]; /* center_re hi/lo, center_im hi/lo, scale hi/lo, max_iter */
    unsigned char pixels[MBROT_PIXELS];
    int updated;
};

/* History for charts (persists across SSE clients) */
#define HIST_MAX 256
extern int hist_best[HIST_MAX];
extern int hist_avg[HIST_MAX];
extern int hist_mut[HIST_MAX];
extern int hist_count;

extern struct tetris_state tetris_state;
extern struct mbrot_state mbrot_state;

void cluster_init(void);
void cluster_handle(const char *frame, int len);
void cluster_register_sse(struct tcp_conn *conn, int type);
void cluster_push_sse(void);
void cluster_poll(void);
void cluster_handle_render_request(struct tcp_conn *conn, const char *body);

#endif
