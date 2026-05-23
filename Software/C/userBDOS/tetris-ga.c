/*
 * tetris-ga.c — Standalone Tetris GA (userBDOS).
 *
 * Runs a genetic algorithm that evolves AI Tetris players.
 * Single population of 20 chromosomes, 6 genes (Q16.16).
 * Sends TETRIS_BOARD snapshots and TETRIS_GA_STATUS to the
 * fpgc-frontend via FNP for live browser visualization.
 */

#include <syscall.h>
#include <fnp.h>

/* ---- Board constants ---- */
#define BOARD_ROWS    22
#define BOARD_COLS    10
#define VISIBLE_ROWS  20

int board[BOARD_ROWS];

/* ---- Tetromino constants ---- */
#define NUM_PIECES 7

int piece_num_rots[7] = {4, 4, 4, 1, 4, 4, 4};
int piece_rows[7] = {4, 3, 3, 3, 3, 3, 3};
int piece_cols[7] = {4, 3, 3, 4, 3, 3, 3};

int t_i[64] = {
  0,0,0,0, 0,0,0,0, 1,1,1,1, 0,0,0,0,
  0,0,1,0, 0,0,1,0, 0,0,1,0, 0,0,1,0,
  0,0,0,0, 1,1,1,1, 0,0,0,0, 0,0,0,0,
  0,1,0,0, 0,1,0,0, 0,1,0,0, 0,1,0,0
};
int t_j[36] = {
  0,0,0, 1,1,1, 1,0,0,
  0,1,0, 0,1,0, 0,1,1,
  0,0,1, 1,1,1, 0,0,0,
  1,1,0, 0,1,0, 0,1,0
};
int t_l[36] = {
  0,0,0, 1,1,1, 0,0,1,
  0,1,1, 0,1,0, 0,1,0,
  1,0,0, 1,1,1, 0,0,0,
  0,1,0, 0,1,0, 1,1,0
};
int t_o[12] = {
  0,0,0,0, 0,1,1,0, 0,1,1,0
};
int t_s[36] = {
  0,0,0, 1,1,0, 0,1,1,
  0,0,1, 0,1,1, 0,1,0,
  1,1,0, 0,1,1, 0,0,0,
  0,1,0, 1,1,0, 1,0,0
};
int t_t[36] = {
  0,0,0, 1,1,1, 0,1,0,
  0,1,0, 0,1,1, 0,1,0,
  0,1,0, 1,1,1, 0,0,0,
  0,1,0, 1,1,0, 0,1,0
};
int t_z[36] = {
  0,0,0, 0,1,1, 1,1,0,
  0,1,0, 0,1,1, 0,0,1,
  0,1,1, 1,1,0, 0,0,0,
  1,0,0, 1,1,0, 0,1,0
};

int *piece_data[7];

/* ---- Game state ---- */
int current_piece;
int next_piece;
int piece_row;
int piece_col;
int piece_rot;

int game_score;
int game_lines;
int game_pieces;
int game_over;

/* ---- AI target ---- */
int ai_target_row;
int ai_target_col;
int ai_target_rot;

/* ---- AI weights (Q16.16) ---- */
int w_lines;
int w_delta_height;
int w_holes;
int w_big_wells;
int w_max_hole_dist;
int w_bumpiness;

/* ---- 7-bag randomizer ---- */
int bag[7];
int bag_index;
unsigned int rng_state;

/* ---- AI placement arrays ---- */
int test_board[BOARD_ROWS];
#define MAX_POSITIONS 60
int pos_row[MAX_POSITIONS];
int pos_col[MAX_POSITIONS];
int pos_rot[MAX_POSITIONS];
int num_positions;

/* ---- Color board (20 × 10 = 200 cells) ---- */
int board_colors[VISIBLE_ROWS * BOARD_COLS];

/* ---- GA constants ---- */
#define NUM_GENES       6
#define POP_SIZE        20
#define BOARD_SEND_INTERVAL 3

/* ---- GA chromosome arrays ---- */
int chromo_genes[POP_SIZE * NUM_GENES];
int chromo_score[POP_SIZE];
int chromo_lines[POP_SIZE];
int chromo_pieces[POP_SIZE];

/* ---- GA state ---- */
int generation;
unsigned int gen_seed;
unsigned int ga_rng_state;
int best_ever_score;
int best_ever_genes[NUM_GENES];
int mutation_count;
int current_chromo;  /* 1-based index of currently playing chromosome */

/* ---- Network ---- */
char frame_buf[FNP_FRAME_BUF_SIZE];
int tx_seq;

/* Frontend MAC (Device 2: 02:B4:B4:00:00:02) */
int frontend_mac[6] = { 0x02, 0xB4, 0xB4, 0x00, 0x00, 0x02 };

/* =========================================================================
 * RNG
 * ========================================================================= */

void rng_seed(unsigned int s)
{
  rng_state = s;
  if (rng_state == 0) rng_state = 1;
}

unsigned int rng_next(void)
{
  rng_state = rng_state * 1103515245 + 12345;
  return rng_state;
}

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

/* Random Q16.16 in [-2.0, +2.0] = [-131072, +131072] */
int ga_random_weight(void)
{
  unsigned int r;
  int val;
  r = ga_rng_next();
  val = (int)(r % 262145) - 131072;
  return val;
}

/* =========================================================================
 * 7-bag
 * ========================================================================= */

void shuffle_bag(void)
{
  int i;
  int j;
  int tmp;
  for (i = 0; i < 7; i++) bag[i] = i;
  for (i = 6; i > 0; i--)
  {
    j = rng_next() % (i + 1);
    tmp = bag[i];
    bag[i] = bag[j];
    bag[j] = tmp;
  }
  bag_index = 0;
}

int next_from_bag(void)
{
  int p;
  p = bag[bag_index];
  bag_index = bag_index + 1;
  if (bag_index >= 7) shuffle_bag();
  return p;
}

/* =========================================================================
 * Board operations
 * ========================================================================= */

void clear_board(void)
{
  int i;
  for (i = 0; i < BOARD_ROWS; i++) board[i] = 0;
  for (i = 0; i < VISIBLE_ROWS * BOARD_COLS; i++) board_colors[i] = 0;
}

int piece_cell(int piece, int rot, int r, int c)
{
  int rows;
  int cols;
  int *data;
  rows = piece_rows[piece];
  cols = piece_cols[piece];
  data = piece_data[piece];
  return data[rot * rows * cols + r * cols + c];
}

int collides(int *brd, int piece, int row, int col, int rot)
{
  int r;
  int c;
  int mr;
  int mc;
  int rows;
  int cols;
  rows = piece_rows[piece];
  cols = piece_cols[piece];
  for (r = 0; r < rows; r++)
  {
    for (c = 0; c < cols; c++)
    {
      if (piece_cell(piece, rot, r, c))
      {
        mr = row + r;
        mc = col + c;
        if (mc < 0 || mc >= BOARD_COLS || mr < 0) return 1;
        if (mr >= BOARD_ROWS) continue;
        if (brd[mr] & (1 << mc)) return 1;
      }
    }
  }
  return 0;
}

void place_piece(int *brd, int piece, int row, int col, int rot)
{
  int r;
  int c;
  int mr;
  int mc;
  int rows;
  int cols;
  rows = piece_rows[piece];
  cols = piece_cols[piece];
  for (r = 0; r < rows; r++)
  {
    for (c = 0; c < cols; c++)
    {
      if (piece_cell(piece, rot, r, c))
      {
        mr = row + r;
        mc = col + c;
        if (mr >= 0 && mr < BOARD_ROWS && mc >= 0 && mc < BOARD_COLS)
        {
          brd[mr] = brd[mr] | (1 << mc);
          if (brd == board && mr < VISIBLE_ROWS)
          {
            board_colors[mr * BOARD_COLS + mc] = piece + 1;
          }
        }
      }
    }
  }
}

int clear_lines(int *brd)
{
  int row;
  int dst;
  int cleared;
  int col;
  cleared = 0;
  dst = 0;
  for (row = 0; row < BOARD_ROWS; row++)
  {
    if ((brd[row] & 0x3FF) == 0x3FF)
    {
      cleared = cleared + 1;
    }
    else
    {
      brd[dst] = brd[row];
      if (brd == board && dst < VISIBLE_ROWS)
      {
        if (row < VISIBLE_ROWS)
        {
          for (col = 0; col < BOARD_COLS; col++)
          {
            board_colors[dst * BOARD_COLS + col] = board_colors[row * BOARD_COLS + col];
          }
        }
        else
        {
          for (col = 0; col < BOARD_COLS; col++)
          {
            board_colors[dst * BOARD_COLS + col] = 0;
          }
        }
      }
      dst = dst + 1;
    }
  }
  while (dst < BOARD_ROWS)
  {
    brd[dst] = 0;
    if (brd == board && dst < VISIBLE_ROWS)
    {
      for (col = 0; col < BOARD_COLS; col++)
      {
        board_colors[dst * BOARD_COLS + col] = 0;
      }
    }
    dst = dst + 1;
  }
  return cleared;
}

int score_for_lines(int n)
{
  if (n == 1) return 40;
  if (n == 2) return 100;
  if (n == 3) return 300;
  if (n == 4) return 1200;
  return 0;
}

/* =========================================================================
 * Heuristics
 * ========================================================================= */

void compute_col_heights(int *brd, int *heights)
{
  int col;
  int row;
  for (col = 0; col < BOARD_COLS; col++)
  {
    heights[col] = 0;
    for (row = BOARD_ROWS - 1; row >= 0; row--)
    {
      if (brd[row] & (1 << col))
      {
        heights[col] = row + 1;
        break;
      }
    }
  }
}

int h_lines_cleared(int *brd)
{
  int row;
  int count;
  count = 0;
  for (row = 0; row < BOARD_ROWS; row++)
  {
    if ((brd[row] & 0x3FF) == 0x3FF) count = count + 1;
  }
  return count;
}

int h_height_diff(int *heights)
{
  int i;
  int mx;
  int mn;
  mx = heights[0];
  mn = heights[0];
  for (i = 1; i < BOARD_COLS; i++)
  {
    if (heights[i] > mx) mx = heights[i];
    if (heights[i] < mn) mn = heights[i];
  }
  return mx - mn;
}

int h_holes(int *brd, int *heights)
{
  int col;
  int row;
  int count;
  count = 0;
  for (col = 0; col < BOARD_COLS; col++)
  {
    for (row = 0; row < heights[col]; row++)
    {
      if (!(brd[row] & (1 << col))) count = count + 1;
    }
  }
  return count;
}

int h_big_wells(int *heights)
{
  int col;
  int count;
  int lh;
  int rh;
  count = 0;
  for (col = 0; col < BOARD_COLS; col++)
  {
    lh = (col == 0) ? BOARD_ROWS : heights[col - 1];
    rh = (col == BOARD_COLS - 1) ? BOARD_ROWS : heights[col + 1];
    if (lh > heights[col] + 1 && rh > heights[col] + 1) count = count + 1;
  }
  return count;
}

int h_max_hole_dist(int *brd, int *heights)
{
  int col;
  int row;
  int left_hole;
  int right_hole;
  int has_hole;
  left_hole = BOARD_COLS - 1;
  right_hole = 0;
  for (col = 0; col < BOARD_COLS; col++)
  {
    has_hole = 0;
    for (row = 0; row < heights[col]; row++)
    {
      if (!(brd[row] & (1 << col)))
      {
        has_hole = 1;
        break;
      }
    }
    if (has_hole)
    {
      if (col < left_hole) left_hole = col;
      if (col > right_hole) right_hole = col;
    }
  }
  if (left_hole > right_hole) return 0;
  return right_hole - left_hole;
}

int h_bumpiness(int *heights)
{
  int col;
  int sum;
  int diff;
  sum = 0;
  for (col = 0; col < BOARD_COLS - 1; col++)
  {
    diff = heights[col] - heights[col + 1];
    if (diff < 0) diff = 0 - diff;
    sum = sum + diff;
  }
  return sum;
}

int evaluate_board(int *brd)
{
  int heights[10];
  compute_col_heights(brd, heights);
  return w_lines * h_lines_cleared(brd) +
         w_delta_height * h_height_diff(heights) +
         w_holes * h_holes(brd, heights) +
         w_big_wells * h_big_wells(heights) +
         w_max_hole_dist * h_max_hole_dist(brd, heights) +
         w_bumpiness * h_bumpiness(heights);
}

/* =========================================================================
 * AI placement
 * ========================================================================= */

void generate_placements(int *brd, int piece)
{
  int row;
  int col;
  int rot;
  int max_rot;
  int higher_row;
  int reachable;

  num_positions = 0;
  max_rot = piece_num_rots[piece];

  for (row = -4; row < VISIBLE_ROWS; row++)
  {
    for (col = -4; col < BOARD_COLS + 4; col++)
    {
      for (rot = 0; rot < max_rot; rot++)
      {
        if (num_positions >= MAX_POSITIONS) return;
        if (!collides(brd, piece, row, col, rot) &&
            collides(brd, piece, row - 1, col, rot))
        {
          reachable = 1;
          for (higher_row = row; higher_row < BOARD_ROWS - 4; higher_row++)
          {
            if (collides(brd, piece, higher_row, col, rot))
            {
              reachable = 0;
              break;
            }
          }
          if (reachable)
          {
            pos_row[num_positions] = row;
            pos_col[num_positions] = col;
            pos_rot[num_positions] = rot;
            num_positions = num_positions + 1;
          }
        }
      }
    }
  }
}

void calculate_best_placement(int *brd, int piece)
{
  int i;
  int r;
  int best_score;
  int score;
  int best_idx;

  generate_placements(brd, piece);

  best_score = 0x80000000;
  best_idx = 0;

  for (i = 0; i < num_positions; i++)
  {
    for (r = 0; r < BOARD_ROWS; r++) test_board[r] = brd[r];
    place_piece(test_board, piece, pos_row[i], pos_col[i], pos_rot[i]);
    score = evaluate_board(test_board);
    if (score > best_score)
    {
      best_score = score;
      best_idx = i;
    }
  }

  if (num_positions > 0)
  {
    ai_target_row = pos_row[best_idx];
    ai_target_col = pos_col[best_idx];
    ai_target_rot = pos_rot[best_idx];
  }
  else
  {
    ai_target_row = piece_row;
    ai_target_col = piece_col;
    ai_target_rot = piece_rot;
  }
}

/* =========================================================================
 * Game operations
 * ========================================================================= */

void spawn_piece(void)
{
  current_piece = next_piece;
  next_piece = next_from_bag();
  piece_row = 17;
  piece_col = 3;
  piece_rot = 0;
  game_pieces = game_pieces + 1;

  if (collides(board, current_piece, piece_row, piece_col, piece_rot))
  {
    game_over = 1;
    return;
  }
  calculate_best_placement(board, current_piece);
}

void hard_drop_and_land(void)
{
  int lines_cleared;
  piece_col = ai_target_col;
  piece_rot = ai_target_rot;
  piece_row = ai_target_row;
  place_piece(board, current_piece, piece_row, piece_col, piece_rot);
  game_score = game_score + 1;
  lines_cleared = clear_lines(board);
  if (lines_cleared > 0)
  {
    game_score = game_score + score_for_lines(lines_cleared);
    game_lines = game_lines + lines_cleared;
  }
}

void init_game(void)
{
  clear_board();
  rng_seed(gen_seed);
  shuffle_bag();
  current_piece = 7; /* T_NONE */
  next_piece = next_from_bag();
  piece_row = 0;
  piece_col = 0;
  piece_rot = 0;
  game_score = 0;
  game_lines = 0;
  game_pieces = 0;
  game_over = 0;
}

/* =========================================================================
 * Network helpers
 * ========================================================================= */

void write_u32(char *buf, int offset, unsigned int val)
{
  buf[offset]     = (val >> 24) & 0xFF;
  buf[offset + 1] = (val >> 16) & 0xFF;
  buf[offset + 2] = (val >> 8) & 0xFF;
  buf[offset + 3] = val & 0xFF;
}

/* Send board snapshot to frontend */
void send_board_snapshot(void)
{
  char data[252];
  int row;
  int col;
  int i;
  int g;

  i = 0;
  for (row = 0; row < VISIBLE_ROWS; row++)
  {
    for (col = 0; col < BOARD_COLS; col++)
    {
      data[i] = board_colors[row * BOARD_COLS + col] & 0xFF;
      i = i + 1;
    }
  }
  write_u32(data, 200, (unsigned int)game_score);
  write_u32(data, 204, (unsigned int)game_lines);
  write_u32(data, 208, (unsigned int)game_pieces);
  write_u32(data, 212, (unsigned int)generation);
  write_u32(data, 216, (unsigned int)current_chromo);
  write_u32(data, 220, (unsigned int)POP_SIZE);
  write_u32(data, 224, (unsigned int)best_ever_score);
  for (g = 0; g < NUM_GENES; g++)
  {
    write_u32(data, 228 + g * 4, (unsigned int)best_ever_genes[g]);
  }

  fnp_send(frontend_mac, FNP_TYPE_TETRIS_BOARD, tx_seq, 0,
           data, 252, frame_buf);
  tx_seq = tx_seq + 1;
}

/* Send GA status to frontend (52 bytes: 28 stats + 24 best genes) */
void send_ga_status(int round_top, int avg)
{
  char data[52];
  int g;

  write_u32(data, 0, (unsigned int)generation);
  write_u32(data, 4, (unsigned int)round_top);
  write_u32(data, 8, (unsigned int)best_ever_score);
  write_u32(data, 12, (unsigned int)best_ever_score);
  write_u32(data, 16, (unsigned int)avg);
  write_u32(data, 20, (unsigned int)mutation_count);
  write_u32(data, 24, (unsigned int)POP_SIZE);
  for (g = 0; g < NUM_GENES; g++)
  {
    write_u32(data, 28 + g * 4, (unsigned int)best_ever_genes[g]);
  }

  fnp_send(frontend_mac, FNP_TYPE_TETRIS_GA_STATUS, tx_seq, 0,
           data, 52, frame_buf);
  tx_seq = tx_seq + 1;
}

/* =========================================================================
 * GA: population
 * ========================================================================= */

void init_population(void)
{
  int i;
  int g;
  for (i = 0; i < POP_SIZE; i++)
  {
    for (g = 0; g < NUM_GENES; g++)
    {
      chromo_genes[i * NUM_GENES + g] = ga_random_weight();
    }
    chromo_score[i] = 0;
    chromo_lines[i] = 0;
    chromo_pieces[i] = 0;
  }
}

/* =========================================================================
 * GA: sorting (insertion sort by score, descending)
 * ========================================================================= */

int sorted_idx[POP_SIZE];

void sort_population(void)
{
  int i;
  int j;
  int tmp;

  for (i = 0; i < POP_SIZE; i++) sorted_idx[i] = i;

  for (i = 1; i < POP_SIZE; i++)
  {
    tmp = sorted_idx[i];
    j = i - 1;
    while (j >= 0 && chromo_score[sorted_idx[j]] < chromo_score[tmp])
    {
      sorted_idx[j + 1] = sorted_idx[j];
      j = j - 1;
    }
    sorted_idx[j + 1] = tmp;
  }
}

/* =========================================================================
 * GA: evolution
 * ========================================================================= */

void evolve_population(void)
{
  int winners[POP_SIZE];
  int num_winners;
  int num_children;
  int child_start;
  int shuf[POP_SIZE];
  int i;
  int j;
  int g;
  int a;
  int b;
  int tmp;

  sort_population();

  /* Tournament selection (size 2) */
  for (i = 0; i < POP_SIZE; i++) shuf[i] = i;
  for (i = POP_SIZE - 1; i > 0; i--)
  {
    j = ga_rng_next() % (i + 1);
    tmp = shuf[i];
    shuf[i] = shuf[j];
    shuf[j] = tmp;
  }

  num_winners = 0;
  for (i = 0; i < POP_SIZE - 1; i = i + 2)
  {
    a = sorted_idx[shuf[i]];
    b = sorted_idx[shuf[i + 1]];
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

  /* Crossover: pair winners to produce children replacing worst */
  num_children = num_winners / 2;
  child_start = POP_SIZE - num_children;

  for (i = 0; i < num_children; i++)
  {
    int p1;
    int p2;
    int child_idx;

    p1 = winners[i * 2];
    p2 = winners[i * 2 + 1];
    child_idx = sorted_idx[child_start + i];

    /* Uniform crossover per gene */
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

    /* Child mutation: 10% per gene */
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
  }

  /* General mutation: 5% per gene for non-elite (top 2 protected) */
  for (i = 2; i < POP_SIZE; i++)
  {
    int idx;
    idx = sorted_idx[i];
    for (g = 0; g < NUM_GENES; g++)
    {
      if (ga_rng_next() % 100 < 5)
      {
        chromo_genes[idx * NUM_GENES + g] = ga_random_weight();
        mutation_count = mutation_count + 1;
      }
    }
  }
}

/* =========================================================================
 * Print helpers
 * ========================================================================= */

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

/* =========================================================================
 * Main
 * ========================================================================= */

int main(void)
{
  int our_mac[6];
  int keys;
  int chromo_idx;
  int g;
  int round_top;
  int total_score;
  int avg_score;
  current_chromo = 0;

  /* Initialize piece data pointers */
  piece_data[0] = t_i;
  piece_data[1] = t_j;
  piece_data[2] = t_l;
  piece_data[3] = t_o;
  piece_data[4] = t_s;
  piece_data[5] = t_t;
  piece_data[6] = t_z;

  /* Initialize FNP */
  fnp_init();
  fnp_get_our_mac(our_mac);
  tx_seq = 0;

  /* Initialize GA */
  ga_rng_seed(12345);
  generation = 1;
  gen_seed = 42;
  best_ever_score = 0;
  mutation_count = 0;
  for (g = 0; g < NUM_GENES; g++) best_ever_genes[g] = 0;
  init_population();

  sys_putstr("Tetris GA started (pop=");
  print_int(POP_SIZE);
  sys_putstr(")\n");

  /* ---- Main loop: run generations forever ---- */
  while (1)
  {
    keys = sys_get_key_state();
    if (keys & KEYSTATE_ESCAPE) break;

    sys_putstr("Gen ");
    print_int(generation);
    sys_putstr(": ");

    round_top = 0;
    total_score = 0;
    mutation_count = 0;

    /* Evaluate all chromosomes */
    for (chromo_idx = 0; chromo_idx < POP_SIZE; chromo_idx++)
    {
      current_chromo = chromo_idx + 1;

      /* Check for escape between games */
      keys = sys_get_key_state();
      if (keys & KEYSTATE_ESCAPE) goto done;

      /* Load weights */
      w_lines        = chromo_genes[chromo_idx * NUM_GENES + 0];
      w_delta_height = chromo_genes[chromo_idx * NUM_GENES + 1];
      w_holes        = chromo_genes[chromo_idx * NUM_GENES + 2];
      w_big_wells    = chromo_genes[chromo_idx * NUM_GENES + 3];
      w_max_hole_dist= chromo_genes[chromo_idx * NUM_GENES + 4];
      w_bumpiness    = chromo_genes[chromo_idx * NUM_GENES + 5];

      /* Play a game */
      init_game();
      spawn_piece();

      while (!game_over)
      {
        hard_drop_and_land();

        /* Send board snapshot periodically */
        if (game_pieces % BOARD_SEND_INTERVAL == 0)
        {
          send_board_snapshot();
          sys_yield();
        }

        if (!game_over)
        {
          spawn_piece();
        }
      }

      /* Send final board snapshot */
      send_board_snapshot();

      /* Record result */
      chromo_score[chromo_idx] = game_score;
      chromo_lines[chromo_idx] = game_lines;
      chromo_pieces[chromo_idx] = game_pieces;

      total_score = total_score + game_score;
      if (game_score > round_top) round_top = game_score;
      if (game_score > best_ever_score)
      {
        best_ever_score = game_score;
        for (g = 0; g < NUM_GENES; g++)
        {
          best_ever_genes[g] = chromo_genes[chromo_idx * NUM_GENES + g];
        }
      }
    }

    /* Generation complete */
    avg_score = total_score / POP_SIZE;

    /* Evolve (updates mutation_count) */
    evolve_population();

    sys_putstr("top=");
    print_int(round_top);
    sys_putstr(" avg=");
    print_int(avg_score);
    sys_putstr(" best=");
    print_int(best_ever_score);
    sys_putstr(" mut=");
    print_int(mutation_count);
    sys_putstr("\n");

    /* Send GA status to frontend (after evolve so mutation_count is set) */
    send_ga_status(round_top, avg_score);

    /* Advance generation counter AFTER reporting, so gen N's results
       are reported as gen N (not gen N+1). */
    generation = generation + 1;
    gen_seed = gen_seed + 1;
  }

done:
  sys_write(1, "\x1b[2J\x1b[H", 7);
  return 0;
}
