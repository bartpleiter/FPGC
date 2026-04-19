/*
 * tetrish.c — Tetris GA coordinator (userBDOS).
 */

//
// tetrish.c — Tetris GA coordinator (userBDOS).
// Manages a 4-island genetic algorithm across 4 worker FPGCs.
// Displays all 4 boards and GA status on the host screen.
//

#include <syscall.h>
#include <fnp.h>
#include <plot.h>

// ---- ANSI shims for retired terminal syscalls ----
// Implemented via sys_write(1, ...) so call sites can stay unchanged.
// The raw /dev/tty fd backs the keyboard helpers.

static int g_tty_fd = -1;

static int append_uint_dec(char *out, int len, int val)
{
  char tmp[12];
  int  i = 0;
  if (val == 0) { tmp[i++] = '0'; }
  else {
    while (val > 0) { tmp[i++] = (char)('0' + (val % 10)); val /= 10; }
  }
  while (i > 0) { out[len++] = tmp[--i]; }
  return len;
}

static void sys_term_set_cursor(int x, int y)
{
  char buf[16];
  int  n = 0;
  buf[n++] = '\x1b';
  buf[n++] = '[';
  n = append_uint_dec(buf, n, y + 1);
  buf[n++] = ';';
  n = append_uint_dec(buf, n, x + 1);
  buf[n++] = 'H';
  sys_write(1, buf, n);
}

static void sys_term_clear(void)
{
  sys_write(1, "\x1b[2J\x1b[H", 7);
}

// ---- Screen constants ----
#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240

// ---- Board constants ----
#define BOARD_ROWS    22
#define BOARD_COLS    10
#define VISIBLE_ROWS  20

// ---- GA constants ----
#define NUM_WORKERS     4
#define NUM_GENES       6
#define ISLAND_SIZE     20
#define TOTAL_CHROMOS   (NUM_WORKERS * ISLAND_SIZE)  // 40
#define ELITE_ISLAND    3   // Worker 3 (device 5) is elite

// ---- Chromosome ----
// Flat arrays to avoid struct issues.
// Index = island * ISLAND_SIZE + chromo_within_island
int chromo_genes[TOTAL_CHROMOS * NUM_GENES]; // 40 × 6 = 240 Q16.16 weights
int chromo_score[TOTAL_CHROMOS];
int chromo_lines[TOTAL_CHROMOS];
int chromo_pieces[TOTAL_CHROMOS];
int chromo_evaluated[TOTAL_CHROMOS]; // 1 if result received

// ---- Worker state ----
// Current chromosome index being played by each worker
int worker_chromo[NUM_WORKERS];
// Next chromosome index to assign to each worker
int worker_next[NUM_WORKERS];
// 1 if worker has finished all its chromosomes
int worker_done[NUM_WORKERS];
// 1 if worker needs a new assignment (set by result processing, cleared after sending)
int worker_needs_assign[NUM_WORKERS];

// ---- Latest board state from each worker (for display) ----
// Color board: 4 workers × 20 visible rows × 10 cols = 800 cells
// Each cell stores piece type: 0=empty, 1-7=piece type
int worker_board[NUM_WORKERS * VISIBLE_ROWS * BOARD_COLS];
int prev_worker_board[NUM_WORKERS * VISIBLE_ROWS * BOARD_COLS]; // shadow buffer for diff rendering
int worker_cur_score[NUM_WORKERS];
int worker_cur_lines[NUM_WORKERS];
int worker_cur_pieces[NUM_WORKERS];

// Piece color palette (RRRGGGBB): index 0=empty(black), 1-7=piece types
int piece_colors[8];

// ---- GA state ----
int generation;
unsigned int gen_seed;
int total_results;
int ga_running;

// ---- RNG for GA operations ----
unsigned int ga_rng_state;

// ---- Display constants ----
#define CELL_SIZE     5
// 4 boards: each 50px wide (10 cols × 5px), 100px tall (20 rows × 5px)
// Centered horizontally, board centers aligned to terminal char grid
#define BOARD_Y       7
int board_x[4] = {39, 103, 167, 231};

// Terminal column positions for display under each board
int score_col[4] = {5, 13, 21, 29};
int gene_col[4] = {6, 14, 22, 30};

// Page navigation
int current_page;
#define NUM_PAGES 4

// All-time best tracking
int best_ever_score;
int best_ever_genes[NUM_GENES];

// Log ring buffer (structured: type + value per entry)
#define LOG_LINES 13
#define LOG_TYPE_HISCORE   1
#define LOG_TYPE_GEN_DONE  2
#define LOG_TYPE_UPDATING  3
#define LOG_TYPE_MUTATIONS 4
#define LOG_TYPE_GEN_START 5
int log_type[LOG_LINES];
int log_value[LOG_LINES];
int log_head;
int log_used;
int log_dirty;

// Mutation counter (reset per evolution cycle)
int mutation_count;

// Generation history for graphs (ring buffer)
#define HIST_MAX 256
int hist_mutations[HIST_MAX];
int hist_avg_score[HIST_MAX];
int hist_top_score[HIST_MAX];
int hist_count;
int graphs_dirty;

// ---- Network ----
char frame_buf[FNP_FRAME_BUF_SIZE];
int tx_seq;

// Worker MAC addresses (flat, 6 ints per worker)
int worker_mac_data[24] = {
  0x02, 0xB4, 0xB4, 0x00, 0x00, 0x02,
  0x02, 0xB4, 0xB4, 0x00, 0x00, 0x03,
  0x02, 0xB4, 0xB4, 0x00, 0x00, 0x04,
  0x02, 0xB4, 0xB4, 0x00, 0x00, 0x05
};

int *get_worker_mac(int w)
{
  return &worker_mac_data[w * 6];
}

// ===========================================================================
// Network helpers
// ===========================================================================

void write_u32(char *buf, int offset, unsigned int val)
{
  buf[offset]     = (val >> 24) & 0xFF;
  buf[offset + 1] = (val >> 16) & 0xFF;
  buf[offset + 2] = (val >> 8) & 0xFF;
  buf[offset + 3] = val & 0xFF;
}

unsigned int read_u32(char *buf, int offset)
{
  return ((buf[offset] & 0xFF) << 24) |
         ((buf[offset + 1] & 0xFF) << 16) |
         ((buf[offset + 2] & 0xFF) << 8) |
         (buf[offset + 3] & 0xFF);
}

// ===========================================================================
// RNG for GA
// ===========================================================================

void ga_rng_seed(unsigned int s)
{
  ga_rng_state = s;
  if (ga_rng_state == 0) ga_rng_state = 1;
}

unsigned int ga_rng_next(void)
{
  ga_rng_state = ga_rng_state * 1103515245 + 12345;
  return ga_rng_state;
}

// Random Q16.16 in [-2.0, +2.0] = [-131072, +131072]
int ga_random_weight(void)
{
  unsigned int r;
  int val;
  r = ga_rng_next();
  // Map to range [0, 262144] then subtract 131072
  val = (int)(r % 262145) - 131072;
  return val;
}

// ===========================================================================
// GA: chromosome access helpers
// ===========================================================================

int *chromo_gene_ptr(int idx)
{
  return &chromo_genes[idx * NUM_GENES];
}

// ===========================================================================
// GA: population initialization
// ===========================================================================

void init_population(void)
{
  int i;
  int g;

  for (i = 0; i < TOTAL_CHROMOS; i++)
  {
    for (g = 0; g < NUM_GENES; g++)
    {
      chromo_genes[i * NUM_GENES + g] = ga_random_weight();
    }
    chromo_score[i] = 0;
    chromo_lines[i] = 0;
    chromo_pieces[i] = 0;
    chromo_evaluated[i] = 0;
  }
}

// ===========================================================================
// GA: sorting (insertion sort by score, descending, within an island)
// ===========================================================================

// Sort indices array by chromo_score[indices[i]], descending
void sort_island(int island_start, int *sorted, int size)
{
  int i;
  int j;
  int tmp;

  // Initialize with 0..size-1
  for (i = 0; i < size; i++) sorted[i] = island_start + i;

  // Insertion sort descending
  for (i = 1; i < size; i++)
  {
    tmp = sorted[i];
    j = i - 1;
    while (j >= 0 && chromo_score[sorted[j]] < chromo_score[tmp])
    {
      sorted[j + 1] = sorted[j];
      j = j - 1;
    }
    sorted[j + 1] = tmp;
  }
}

// ===========================================================================
// GA: evolution for one island
// ===========================================================================

void evolve_island(int island_idx)
{
  int start;
  int sorted[ISLAND_SIZE];
  int winners[ISLAND_SIZE];
  int num_winners;
  int num_children;
  int i;
  int g;
  int a;
  int b;
  int child_start;

  start = island_idx * ISLAND_SIZE;

  // Sort by fitness (descending)
  sort_island(start, sorted, ISLAND_SIZE);

  // Tournament selection (size 2): pair sorted indices randomly
  // Shuffle for random pairing
  {
    int shuf[ISLAND_SIZE];
    int tmp;
    int j;
    for (i = 0; i < ISLAND_SIZE; i++) shuf[i] = i;
    for (i = ISLAND_SIZE - 1; i > 0; i--)
    {
      j = ga_rng_next() % (i + 1);
      tmp = shuf[i];
      shuf[i] = shuf[j];
      shuf[j] = tmp;
    }

    num_winners = 0;
    for (i = 0; i < ISLAND_SIZE - 1; i = i + 2)
    {
      a = sorted[shuf[i]];
      b = sorted[shuf[i + 1]];
      if (chromo_score[a] >= chromo_score[b])
      {
        winners[num_winners] = a;
      }
      else
      {
        winners[num_winners] = b;
      }
      num_winners = num_winners + 1;
    }
  }

  // Crossover: pair winners to produce children
  num_children = num_winners / 2;
  // Children will replace the worst chromosomes
  child_start = ISLAND_SIZE - num_children;

  for (i = 0; i < num_children; i++)
  {
    int p1;
    int p2;
    int child_idx;

    p1 = winners[i * 2];
    p2 = winners[i * 2 + 1];
    child_idx = sorted[child_start + i]; // Replace worst

    // Uniform crossover per gene
    for (g = 0; g < NUM_GENES; g++)
    {
      if (ga_rng_next() % 2 == 0)
      {
        chromo_genes[child_idx * NUM_GENES + g] = chromo_genes[p1 * NUM_GENES + g];
      }
      else
      {
        chromo_genes[child_idx * NUM_GENES + g] = chromo_genes[p2 * NUM_GENES + g];
      }
    }

    // Child mutation: 10% per gene
    for (g = 0; g < NUM_GENES; g++)
    {
      if (ga_rng_next() % 100 < 10)
      {
        chromo_genes[child_idx * NUM_GENES + g] = ga_random_weight();
        mutation_count = mutation_count + 1;
      }
    }

    chromo_score[child_idx] = 0;
    chromo_lines[child_idx] = 0;
    chromo_pieces[child_idx] = 0;
    chromo_evaluated[child_idx] = 0;
  }

  // General mutation: 5% per gene for all non-elite chromosomes
  // Top 2 are elite (protected)
  for (i = 2; i < ISLAND_SIZE; i++)
  {
    int idx;
    idx = sorted[i];
    for (g = 0; g < NUM_GENES; g++)
    {
      if (ga_rng_next() % 100 < 5)
      {
        chromo_genes[idx * NUM_GENES + g] = ga_random_weight();
        chromo_evaluated[idx] = 0;
        mutation_count = mutation_count + 1;
      }
    }
  }
}

// ===========================================================================
// GA: migration between islands
// ===========================================================================

void do_migration(void)
{
  int island;
  int elite_start;
  int sorted_elite[ISLAND_SIZE];
  int sorted_normal[ISLAND_SIZE];
  int best_idx;
  int worst_elite_idx;
  int random_elite_idx;
  int worst_normal_idx;
  int g;

  elite_start = ELITE_ISLAND * ISLAND_SIZE;

  // Sort elite island
  sort_island(elite_start, sorted_elite, ISLAND_SIZE);

  // For each normal island (0, 1, 2): migrate best → elite, random elite → normal
  for (island = 0; island < ELITE_ISLAND; island++)
  {
    int normal_start;
    normal_start = island * ISLAND_SIZE;

    // Sort normal island
    sort_island(normal_start, sorted_normal, ISLAND_SIZE);

    // Best from normal island
    best_idx = sorted_normal[0];
    // Worst in elite island
    worst_elite_idx = sorted_elite[ISLAND_SIZE - 1 - island]; // Use different worst slots

    // Copy best normal → worst elite
    for (g = 0; g < NUM_GENES; g++)
    {
      chromo_genes[worst_elite_idx * NUM_GENES + g] =
        chromo_genes[best_idx * NUM_GENES + g];
    }
    chromo_score[worst_elite_idx] = chromo_score[best_idx];
    chromo_lines[worst_elite_idx] = chromo_lines[best_idx];
    chromo_evaluated[worst_elite_idx] = 0; // Needs re-evaluation

    // Random from elite → worst of normal island
    random_elite_idx = sorted_elite[ga_rng_next() % ISLAND_SIZE];
    worst_normal_idx = sorted_normal[ISLAND_SIZE - 1];

    for (g = 0; g < NUM_GENES; g++)
    {
      chromo_genes[worst_normal_idx * NUM_GENES + g] =
        chromo_genes[random_elite_idx * NUM_GENES + g];
    }
    chromo_score[worst_normal_idx] = chromo_score[random_elite_idx];
    chromo_lines[worst_normal_idx] = chromo_lines[random_elite_idx];
    chromo_evaluated[worst_normal_idx] = 0;
  }
}

// ===========================================================================
// GA: full generation evolution
// ===========================================================================

void evolve_generation(void)
{
  int i;

  // Evolve each island
  for (i = 0; i < NUM_WORKERS; i++)
  {
    evolve_island(i);
  }

  // Migration
  do_migration();

  // Reset evaluation flags
  for (i = 0; i < TOTAL_CHROMOS; i++)
  {
    chromo_evaluated[i] = 0;
  }

  generation = generation + 1;

  // New seed for piece sequence determinism
  gen_seed = gen_seed + 1;
}

// ===========================================================================
// Worker dispatch
// ===========================================================================

// Send TETRIS_PARAMS to a worker
void send_params(int w)
{
  char data[4];
  write_u32(data, 0, gen_seed);
  fnp_send(get_worker_mac(w), FNP_TYPE_TETRIS_PARAMS, tx_seq, 0,
           data, 4, frame_buf);
  tx_seq = tx_seq + 1;
}

// Send TETRIS_ASSIGN to a worker
void send_assign(int w, int chromo_idx)
{
  char data[28];
  int *genes;
  int g;

  genes = chromo_gene_ptr(chromo_idx);
  write_u32(data, 0, (unsigned int)chromo_idx);
  for (g = 0; g < NUM_GENES; g++)
  {
    write_u32(data, 4 + g * 4, (unsigned int)genes[g]);
  }

  fnp_send(get_worker_mac(w), FNP_TYPE_TETRIS_ASSIGN, tx_seq, 0,
           data, 28, frame_buf);
  tx_seq = tx_seq + 1;
}

// Initialize worker dispatch for a new generation
void dispatch_generation(void)
{
  int w;

  for (w = 0; w < NUM_WORKERS; w++)
  {
    worker_next[w] = w * ISLAND_SIZE; // First chromosome in this island
    worker_done[w] = 0;
    worker_needs_assign[w] = 0;

    // Send params + first assignment
    send_params(w);
    send_assign(w, worker_next[w]);
    worker_chromo[w] = worker_next[w];
    worker_next[w] = worker_next[w] + 1;
  }

  total_results = 0;
}

// Assign next chromosome to a worker, or mark it done
void assign_next_to_worker(int w)
{
  int island_end;
  island_end = (w + 1) * ISLAND_SIZE;

  if (worker_next[w] < island_end)
  {
    send_assign(w, worker_next[w]);
    worker_chromo[w] = worker_next[w];
    worker_next[w] = worker_next[w] + 1;
  }
  else
  {
    worker_done[w] = 1;
  }
}

// ===========================================================================
// Worker identification
// ===========================================================================

int identify_worker(int *src_mac)
{
  int last_byte;
  last_byte = src_mac[5] & 0xFF;
  if (last_byte >= 0x02 && last_byte <= 0x05)
  {
    if (src_mac[0] == 0x02 && src_mac[1] == 0xB4 && src_mac[2] == 0xB4 &&
        src_mac[3] == 0x00 && src_mac[4] == 0x00)
    {
      return last_byte - 0x02;
    }
  }
  return -1;
}

// ===========================================================================
// Network receive
// ===========================================================================

void process_board_msg(int worker_id, char *data, int data_len)
{
  int i;
  int base;
  int cells;

  if (data_len < 212) return;

  // 200 bytes of color data (20 rows × 10 cols)
  base = worker_id * VISIBLE_ROWS * BOARD_COLS;
  cells = VISIBLE_ROWS * BOARD_COLS;
  for (i = 0; i < cells; i++)
  {
    worker_board[base + i] = data[i] & 0xFF;
  }
  worker_cur_score[worker_id]  = (int)read_u32(data, 200);
  worker_cur_lines[worker_id]  = (int)read_u32(data, 204);
  worker_cur_pieces[worker_id] = (int)read_u32(data, 208);
}

void process_result_msg(int worker_id, char *data, int data_len)
{
  int idx;

  if (data_len < 16) return;

  idx = (int)read_u32(data, 0);
  if (idx < 0 || idx >= TOTAL_CHROMOS) return;

  chromo_score[idx]     = (int)read_u32(data, 4);
  chromo_lines[idx]     = (int)read_u32(data, 8);
  chromo_pieces[idx]    = (int)read_u32(data, 12);
  chromo_evaluated[idx] = 1;
  total_results = total_results + 1;

  // Mark worker as needing next assignment (deferred to avoid sending before ACK)
  worker_needs_assign[worker_id] = 1;
}

// Poll network — returns 1 if any data was received (board or result), 0 otherwise
int poll_network(void)
{
  int rxlen;
  int src_mac[6];
  int msg_type;
  int rx_seq;
  int rx_flags;
  char *rx_data;
  int rx_data_len;
  int worker_id;
  int got_data;
  int need_ack;
  int ack_seq;
  int ack_mac[6];
  char ack_data[2];

  got_data = 0;

  while (sys_net_packet_count() > 0)
  {
    rxlen = sys_net_recv(frame_buf, FNP_FRAME_BUF_SIZE);
    if (rxlen > 0 && fnp_parse(frame_buf, rxlen, src_mac,
                                &msg_type, &rx_seq, &rx_flags,
                                &rx_data, &rx_data_len))
    {
      need_ack = (rx_flags & FNP_FLAG_REQUIRES_ACK);
      ack_seq = rx_seq;
      if (need_ack)
      {
        ack_mac[0] = src_mac[0]; ack_mac[1] = src_mac[1];
        ack_mac[2] = src_mac[2]; ack_mac[3] = src_mac[3];
        ack_mac[4] = src_mac[4]; ack_mac[5] = src_mac[5];
      }

      worker_id = identify_worker(src_mac);
      if (worker_id >= 0)
      {
        if (msg_type == FNP_TYPE_TETRIS_BOARD)
        {
          process_board_msg(worker_id, rx_data, rx_data_len);
          got_data = 1;
        }
        else if (msg_type == FNP_TYPE_TETRIS_RESULT)
        {
          process_result_msg(worker_id, rx_data, rx_data_len);
          got_data = 1;
        }
      }

      if (need_ack)
      {
        ack_data[0] = (ack_seq >> 8) & 0xFF;
        ack_data[1] = ack_seq & 0xFF;
        fnp_send(ack_mac, FNP_TYPE_ACK, 0, 0, ack_data, 2, frame_buf);
      }
    }
  }
  return got_data;
}

// Send deferred assignments to workers that finished a game.
// Called AFTER poll_network so ACKs are already sent.
void dispatch_pending_assigns(void)
{
  int w;
  for (w = 0; w < NUM_WORKERS; w++)
  {
    if (worker_needs_assign[w])
    {
      worker_needs_assign[w] = 0;
      assign_next_to_worker(w);
    }
  }
}

// ===========================================================================
// Display: pixel framebuffer
// ===========================================================================

void fb_write(int offset, int color)
{
  __builtin_store(PIXEL_FB_ADDR + offset * 4, color);
}

void draw_rect(int x, int y, int w, int h, int color)
{
  int px;
  int py;
  for (py = y; py < y + h; py++)
  {
    if (py >= 0 && py < SCREEN_HEIGHT)
    {
      for (px = x; px < x + w; px++)
      {
        if (px >= 0 && px < SCREEN_WIDTH)
        {
          fb_write(py * SCREEN_WIDTH + px, color);
        }
      }
    }
  }
}

void draw_worker_board(int w)
{
  int row;
  int col;
  int bx;
  int x;
  int y;
  int base;
  int idx;
  int cell_val;
  int color;

  bx = board_x[w];
  base = w * VISIBLE_ROWS * BOARD_COLS;

  // Only draw cells that changed since last draw
  for (row = 0; row < VISIBLE_ROWS; row++)
  {
    for (col = 0; col < BOARD_COLS; col++)
    {
      idx = base + row * BOARD_COLS + col;
      cell_val = worker_board[idx];
      if (cell_val != prev_worker_board[idx])
      {
        if (cell_val > 0 && cell_val < 8)
        {
          color = piece_colors[cell_val];
        }
        else
        {
          color = 0x00;
        }
        x = bx + col * CELL_SIZE;
        y = BOARD_Y + (VISIBLE_ROWS - 1 - row) * CELL_SIZE;
        draw_rect(x, y, CELL_SIZE - 1, CELL_SIZE - 1, color);
        prev_worker_board[idx] = cell_val;
      }
    }
  }
}

void draw_all_boards(void)
{
  int w;
  for (w = 0; w < NUM_WORKERS; w++)
  {
    draw_worker_board(w);
  }
}

// ===========================================================================
// Display: terminal text (status area below boards)
// ===========================================================================

void print_int(int val)
{
  char buf[12];
  int i;
  int neg;
  unsigned int uval;

  if (val == 0)
  {
    sys_putc('0');
    return;
  }
  neg = 0;
  if (val < 0)
  {
    neg = 1;
    uval = (unsigned int)(0 - val);
  }
  else
  {
    uval = (unsigned int)val;
  }
  i = 11;
  buf[i] = 0;
  while (uval > 0)
  {
    i = i - 1;
    buf[i] = '0' + (uval % 10);
    uval = uval / 10;
  }
  if (neg)
  {
    i = i - 1;
    buf[i] = '-';
  }
  sys_putstr(buf + i);
}

// Pad integer to a fixed width with spaces (right-aligned)
void print_int_pad(int val, int width)
{
  char buf[12];
  int i;
  int neg;
  unsigned int uval;
  int len;

  if (val == 0)
  {
    for (i = 0; i < width - 1; i++) sys_putc(' ');
    sys_putc('0');
    return;
  }

  neg = 0;
  if (val < 0)
  {
    neg = 1;
    uval = (unsigned int)(0 - val);
  }
  else
  {
    uval = (unsigned int)val;
  }

  i = 11;
  buf[i] = 0;
  while (uval > 0)
  {
    i = i - 1;
    buf[i] = '0' + (uval % 10);
    uval = uval / 10;
  }
  if (neg)
  {
    i = i - 1;
    buf[i] = '-';
  }

  len = 11 - i;
  while (len < width)
  {
    sys_putc(' ');
    len = len + 1;
  }
  sys_putstr(buf + i);
}

// Print integer compactly: if it fits in 'width' chars, print normally.
// Otherwise use E notation: "X.YEZ" (5 chars) padded to width.
void print_int_compact(int val, int width)
{
  int ndigits;
  int temp;
  int sig;
  int exp;
  int pad;

  if (val <= 0)
  {
    print_int_pad(val, width);
    return;
  }

  // Count digits
  ndigits = 0;
  temp = val;
  while (temp > 0)
  {
    ndigits = ndigits + 1;
    temp = temp / 10;
  }

  if (ndigits <= width)
  {
    print_int_pad(val, width);
    return;
  }

  // E notation: reduce to 2 significant digits
  exp = ndigits - 1;
  sig = val;
  while (sig >= 100)
  {
    sig = sig / 10;
  }

  // Pad to fill width (5 chars for "X.YEZ")
  pad = width - 5;
  while (pad > 0)
  {
    sys_putc(' ');
    pad = pad - 1;
  }
  sys_putc('0' + (sig / 10));
  sys_putc('.');
  sys_putc('0' + (sig % 10));
  sys_putc('E');
  sys_putc('0' + exp);
}

// Print a Q16.16 fixed-point value as 5 chars: " X.XX" or "-X.XX"
void print_q16(int val)
{
  int neg;
  unsigned int abs_val;
  int int_part;
  int frac;

  neg = 0;
  if (val < 0)
  {
    neg = 1;
    abs_val = (unsigned int)(0 - val);
  }
  else
  {
    abs_val = (unsigned int)val;
  }

  int_part = abs_val >> 16;
  frac = ((abs_val & 0xFFFF) * 100) >> 16;

  if (neg)
  {
    sys_putc('-');
  }
  else
  {
    sys_putc(' ');
  }

  sys_putc('0' + int_part);
  sys_putc('.');
  sys_putc('0' + (frac / 10));
  sys_putc('0' + (frac % 10));
}

void print_gene_name_short(int g)
{
  if (g == 0) sys_putstr("Lines");
  else if (g == 1) sys_putstr("dHght");
  else if (g == 2) sys_putstr("Holes");
  else if (g == 3) sys_putstr("Wells");
  else if (g == 4) sys_putstr("HoleD");
  else if (g == 5) sys_putstr("Bumpy");
}

void print_gene_name_full(int g)
{
  if (g == 0) sys_putstr("Lines Cleared");
  else if (g == 1) sys_putstr("Delta Height");
  else if (g == 2) sys_putstr("Holes");
  else if (g == 3) sys_putstr("Big Wells");
  else if (g == 4) sys_putstr("Max Hole Dist");
  else if (g == 5) sys_putstr("Bumpiness");
}

void log_add(int type, int value)
{
  log_type[log_head] = type;
  log_value[log_head] = value;
  log_head = (log_head + 1) % LOG_LINES;
  if (log_used < LOG_LINES) log_used = log_used + 1;
  log_dirty = 1;
}

void draw_status(void)
{
  int w;
  int g;
  int *genes;
  int round_hi;
  int i;

  // Update all-time best tracking
  for (i = 0; i < TOTAL_CHROMOS; i++)
  {
    if (chromo_evaluated[i] && chromo_score[i] > best_ever_score)
    {
      best_ever_score = chromo_score[i];
      for (g = 0; g < NUM_GENES; g++)
      {
        best_ever_genes[g] = chromo_genes[i * NUM_GENES + g];
      }
      log_add(LOG_TYPE_HISCORE, best_ever_score);
    }
  }

  if (current_page == 0)
  {
    // Row 11: Scores under each board with label
    sys_term_set_cursor(0, 11);
    sys_putstr("Score");
    for (w = 0; w < NUM_WORKERS; w++)
    {
      sys_term_set_cursor(score_col[w], 11);
      print_int_compact(worker_cur_score[w], 6);
    }

    // Rows 13-18: Gene values per worker with short names
    for (g = 0; g < NUM_GENES; g++)
    {
      sys_term_set_cursor(0, 13 + g);
      print_gene_name_short(g);
      for (w = 0; w < NUM_WORKERS; w++)
      {
        genes = chromo_gene_ptr(worker_chromo[w]);
        sys_term_set_cursor(gene_col[w], 13 + g);
        print_q16(genes[g]);
      }
    }

    // Row 20: Games progress
    sys_term_set_cursor(0, 20);
    sys_putstr("Games ");
    print_int_pad(total_results, 2);
    sys_putstr("/");
    print_int(TOTAL_CHROMOS);

    // Row 22: Round high score
    round_hi = 0;
    for (i = 0; i < TOTAL_CHROMOS; i++)
    {
      if (chromo_evaluated[i] && chromo_score[i] > round_hi)
      {
        round_hi = chromo_score[i];
      }
    }
    sys_term_set_cursor(0, 22);
    sys_putstr("Round HiScore ");
    print_int_compact(round_hi, 6);
  }
  else if (current_page == 1)
  {
    // GA Overview page
    sys_term_set_cursor(0, 11);
    sys_putstr("Generation   ");
    print_int_pad(generation, 4);

    sys_term_set_cursor(0, 13);
    sys_putstr("HiScore      ");
    print_int_compact(best_ever_score, 6);

    sys_term_set_cursor(0, 15);
    sys_putstr("Best Genes:");
    for (g = 0; g < NUM_GENES; g++)
    {
      sys_term_set_cursor(2, 16 + g);
      print_gene_name_full(g);
      sys_term_set_cursor(18, 16 + g);
      print_q16(best_ever_genes[g]);
    }
  }
  else if (current_page == 2)
  {
    if (log_dirty)
    {
      int li;
      int start_idx;
      int idx;

      log_dirty = 0;

      // Clear log area to prevent leftover characters
      for (li = 0; li < LOG_LINES; li++)
      {
        sys_term_set_cursor(0, 11 + li);
        sys_putstr("                                        ");
      }

      if (log_used < LOG_LINES)
      {
        start_idx = 0;
      }
      else
      {
        start_idx = log_head;
      }

      for (li = 0; li < log_used; li++)
      {
        idx = (start_idx + li) % LOG_LINES;
        sys_term_set_cursor(0, 11 + li);

        if (log_type[idx] == LOG_TYPE_HISCORE)
        {
          sys_putstr("New HiScore: ");
          print_int(log_value[idx]);
        }
        else if (log_type[idx] == LOG_TYPE_GEN_DONE)
        {
          sys_putstr("Gen ");
          print_int(log_value[idx]);
          sys_putstr(" is finished");
        }
        else if (log_type[idx] == LOG_TYPE_UPDATING)
        {
          sys_putstr("Updating population");
        }
        else if (log_type[idx] == LOG_TYPE_MUTATIONS)
        {
          print_int(log_value[idx]);
          sys_putstr(" Mutations occurred");
        }
        else if (log_type[idx] == LOG_TYPE_GEN_START)
        {
          sys_putstr("Started gen ");
          print_int(log_value[idx]);
        }
      }
    }
  }
  else if (current_page == 3)
  {
    if (graphs_dirty)
    {
      int y_max_mut;
      int y_max_avg;
      int y_max_top;
      int gi;

      graphs_dirty = 0;

      if (hist_count > 1)
      {
        // Find Y ranges (separate for each metric)
        y_max_mut = 1;
        y_max_avg = 1;
        y_max_top = 1;
        for (gi = 0; gi < hist_count; gi++)
        {
          if (hist_mutations[gi] > y_max_mut) y_max_mut = hist_mutations[gi];
          if (hist_avg_score[gi] > y_max_avg) y_max_avg = hist_avg_score[gi];
          if (hist_top_score[gi] > y_max_top) y_max_top = hist_top_score[gi];
        }

        // Plot 1: Mutations (left below boards)
        plot_init(5, 116, 100, 120);
        plot_clear(0x00);
        plot_axes(0, y_max_mut, hist_count, 0xB6, 0x49);
        plot_line(hist_mutations, hist_count, 0, y_max_mut, 0xFC);
        plot_text(30, 117, "MUTATIONS", 0xFF);

        // Plot 2: Avg Score (center below boards)
        plot_init(110, 116, 100, 120);
        plot_clear(0x00);
        plot_axes(0, y_max_avg, hist_count, 0xB6, 0x49);
        plot_line(hist_avg_score, hist_count, 0, y_max_avg, 0x1C);
        plot_text(135, 117, "AVG SCORE", 0xFF);

        // Plot 3: Top Score (right below boards)
        plot_init(215, 116, 100, 120);
        plot_clear(0x00);
        plot_axes(0, y_max_top, hist_count, 0xB6, 0x49);
        plot_line(hist_top_score, hist_count, 0, y_max_top, 0xE0);
        plot_text(240, 117, "TOP SCORE", 0xFF);
      }
      else
      {
        sys_term_set_cursor(2, 18);
        sys_putstr("Waiting for data...");
      }
    }
  }

  // Page navigation indicator (top-left, beside boards)
  sys_term_set_cursor(0, 0);
  if (current_page > 0)
  {
    sys_putc('<');
  }
  else
  {
    sys_putc(' ');
  }
  sys_putc('1' + current_page);
  sys_putc('/');
  sys_putc('0' + NUM_PAGES);
  if (current_page < NUM_PAGES - 1)
  {
    sys_putc('>');
  }
  else
  {
    sys_putc(' ');
  }
}

// ===========================================================================
// Launch workers
// ===========================================================================

void launch_workers(void)
{
  int w;
  for (w = 0; w < NUM_WORKERS; w++)
  {
    fnp_send_command(get_worker_mac(w), "tetrisc", frame_buf, &tx_seq);
  }
  sys_delay(1000);
}

// ===========================================================================
// Framebuffer helpers
// ===========================================================================

void fb_clear(void)
{
  int ci;
  for (ci = 0; ci < SCREEN_WIDTH * SCREEN_HEIGHT; ci++)
  {
    fb_write(ci, 0);
  }
}

void fb_clear_region(int start, int end)
{
  int cy;
  for (cy = start; cy < end; cy++)
  {
    fb_write(cy, 0);
  }
}

// ===========================================================================
// Main
// ===========================================================================

int main(void)
{
  int our_mac[6];
  int keys;
  int key;
  int running;

  // Initialize FNP
  fnp_init();
  fnp_get_our_mac(our_mac);
  tx_seq = 0;

  // Open raw /dev/tty for non-blocking key event polling.
  g_tty_fd = sys_tty_open_raw(1);
  if (g_tty_fd < 0)
  {
    sys_putstr("tetrish: cannot open /dev/tty in raw mode\n");
    return 1;
  }

  // Launch workers
  launch_workers();

  // Clear screen
  sys_term_clear();
  fb_clear();

  // Initialize worker board display
  {
    int i;
    for (i = 0; i < NUM_WORKERS * VISIBLE_ROWS * BOARD_COLS; i++)
    {
      worker_board[i] = 0;
      prev_worker_board[i] = -1; // sentinel: forces initial full draw
    }
    for (i = 0; i < NUM_WORKERS; i++)
    {
      worker_cur_score[i] = 0;
      worker_cur_lines[i] = 0;
      worker_cur_pieces[i] = 0;
    }
  }

  // Piece color palette (RRRGGGBB): 0=black, I=cyan, J=blue, L=orange, O=yellow, S=green, T=purple, Z=red
  piece_colors[0] = 0x00;
  piece_colors[1] = 0x1F; // I - cyan
  piece_colors[2] = 0x03; // J - blue
  piece_colors[3] = 0xF4; // L - orange
  piece_colors[4] = 0xFC; // O - yellow
  piece_colors[5] = 0x1C; // S - green
  piece_colors[6] = 0xA3; // T - purple
  piece_colors[7] = 0xE0; // Z - red

  // Initialize GA
  ga_rng_seed(12345);
  generation = 1;
  gen_seed = 42;
  ga_running = 1;
  current_page = 0;
  best_ever_score = 0;
  log_head = 0;
  log_used = 0;
  log_dirty = 1;
  mutation_count = 0;
  hist_count = 0;
  graphs_dirty = 1;
  {
    int gi;
    for (gi = 0; gi < NUM_GENES; gi++) best_ever_genes[gi] = 0;
  }

  init_population();
  dispatch_generation();

  // Draw board borders once
  {
    int w;
    for (w = 0; w < NUM_WORKERS; w++)
    {
      draw_rect(board_x[w] - 1, BOARD_Y - 1,
                BOARD_COLS * CELL_SIZE + 2, VISIBLE_ROWS * CELL_SIZE + 2, 0xB6);
    }
  }

  // Draw initial display
  draw_all_boards();
  draw_status();

  // ---- Main loop ----
  running = 1;
  while (running)
  {
    // Check keyboard
    while ((key = sys_tty_event_read(g_tty_fd, 0)) >= 0)
    {
      if (key == ' ')
      {
        ga_running = !ga_running;
      }
      else if (key == 'r' || key == 'R')
      {
        // Reset GA
        ga_rng_seed(12345);
        generation = 1;
        gen_seed = 42;
        best_ever_score = 0;
        log_head = 0;
        log_used = 0;
        log_dirty = 1;
        hist_count = 0;
        graphs_dirty = 1;
        init_population();
        dispatch_generation();
      }
      else if (key == KEY_LEFT)
      {
        if (current_page > 0)
        {
          current_page = current_page - 1;
          log_dirty = 1;
          graphs_dirty = 1;
          // Clear plot area if leaving graphs page
          if (current_page + 1 == 3)
          {
            fb_clear_region(116 * 320, 236 * 320);
          }
          sys_term_clear();
          draw_status();
        }
      }
      else if (key == KEY_RIGHT)
      {
        if (current_page < NUM_PAGES - 1)
        {
          current_page = current_page + 1;
          log_dirty = 1;
          graphs_dirty = 1;
          // Clear plot area if leaving graphs page
          if (current_page - 1 == 3)
          {
            fb_clear_region(116 * 320, 236 * 320);
          }
          sys_term_clear();
          draw_status();
        }
      }
    }

    keys = sys_get_key_state();
    if (keys & KEYSTATE_ESCAPE)
    {
      running = 0;
      break;
    }

    if (!ga_running)
    {
      sys_delay(50);
      continue;
    }

    // Poll network
    {
      int got_data;
      got_data = poll_network();

      // Send deferred assignments AFTER ACKs have been sent
      dispatch_pending_assigns();

      if (total_results >= TOTAL_CHROMOS)
      {
        // Record generation history before evolving
        if (hist_count < HIST_MAX)
        {
          int hi;
          int htotal;
          int hcount;
          int htop;

          htotal = 0;
          hcount = 0;
          htop = 0;
          for (hi = 0; hi < TOTAL_CHROMOS; hi++)
          {
            if (chromo_evaluated[hi])
            {
              htotal = htotal + chromo_score[hi];
              hcount = hcount + 1;
              if (chromo_score[hi] > htop) htop = chromo_score[hi];
            }
          }
          hist_avg_score[hist_count] = (hcount > 0) ? htotal / hcount : 0;
          hist_top_score[hist_count] = htop;
        }

        log_add(LOG_TYPE_GEN_DONE, generation);
        log_add(LOG_TYPE_UPDATING, 0);
        mutation_count = 0;
        // Evolve and start next generation
        evolve_generation();
        log_add(LOG_TYPE_MUTATIONS, mutation_count);
        log_add(LOG_TYPE_GEN_START, generation);

        // Record mutation count in history
        if (hist_count < HIST_MAX)
        {
          hist_mutations[hist_count] = mutation_count;
          hist_count = hist_count + 1;
          graphs_dirty = 1;
        }

        dispatch_generation();
        got_data = 1; // Force redraw
      }

      // Only redraw when we received new data
      if (got_data)
      {
        draw_all_boards();
        draw_status();
      }
    }
  }

  // Cleanup
  fb_clear();
  sys_term_clear();

  return 0;
}
