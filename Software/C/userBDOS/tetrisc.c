/*
 * FIXME: shell-terminal-v2 Phase E migration TODO.
 *
 * This program still uses syscalls that were removed from BDOS in
 * Phase E. It will currently fail to link or behave correctly. See
 * the migration table at the top of Software/C/userlib/include/syscall.h.
 *
 * Quick checklist for porting:
 *   sys_term_put_cell / sys_term_clear / sys_term_set_cursor
 *      -> sys_write(1, "\x1b[<y+1>;<x+1>H...", n) ANSI escapes.
 *      -> Glyphs that overlap C0 control codes (BEL, HT, LF, ESC, ...)
 *         must be substituted with printable ASCII.
 *   sys_read_key / sys_key_available
 *      -> int fd = sys_tty_open_raw(1);            (non-blocking)
 *         int ev = sys_tty_event_read(fd, 0);
 *      -> snake.c is the reference port.
 *   sys_set_palette / sys_set_pixel_palette
 *      -> No replacement syscall yet. Either use ANSI SGR colors
 *         (\x1b[30m..37m for tile palette 0..7) or wait for a
 *         dedicated palette syscall to be added in a follow-up.
 *   sys_uart_print_str / sys_uart_print_char
 *      -> sys_write(2, s, n) (stderr; mirrored to UART by libterm v2).
 */

//
// tetrisc.c — Tetris GA worker (userBDOS).
// Receives chromosome weights from the host, plays AI Tetris games,
// periodically sends board snapshots, and reports final scores.
//

#include <syscall.h>
#include <fnp.h>

// ---- Board constants ----
#define BOARD_ROWS    22
#define BOARD_COLS    10
#define VISIBLE_ROWS  20

int board[BOARD_ROWS];

// ---- Tetromino constants ----
#define T_I 0
#define T_J 1
#define T_L 2
#define T_O 3
#define T_S 4
#define T_T 5
#define T_Z 6
#define T_NONE 7
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

// ---- Game state ----
int current_piece;
int next_piece;
int piece_row;
int piece_col;
int piece_rot;

int game_score;
int game_lines;
int game_pieces;
int game_over;

// ---- AI target ----
int ai_target_row;
int ai_target_col;
int ai_target_rot;

// ---- AI weights (Q16.16) ----
int w_lines;
int w_delta_height;
int w_holes;
int w_big_wells;
int w_max_hole_dist;
int w_bumpiness;

// ---- 7-bag randomizer ----
int bag[7];
int bag_index;
unsigned int rng_state;

// ---- AI placement arrays ----
int test_board[BOARD_ROWS];
#define MAX_POSITIONS 60
int pos_row[MAX_POSITIONS];
int pos_col[MAX_POSITIONS];
int pos_rot[MAX_POSITIONS];
int num_positions;

// ---- Network state ----
char frame_buf[FNP_FRAME_BUF_SIZE];
int tx_seq;
int coord_mac[6];

// ---- Assignment state ----
unsigned int gen_seed;
int chromo_id;
int has_params;
int has_assign;

// ---- Board send interval (every N pieces) ----
#define BOARD_SEND_INTERVAL 1

// ---- Color board for display: stores piece type + 1 (0 = empty, 1-7 = piece) ----
// Only visible rows (20 × 10 = 200 cells)
int board_colors[VISIBLE_ROWS * BOARD_COLS];

// ===========================================================================
// RNG
// ===========================================================================

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

// ===========================================================================
// 7-bag
// ===========================================================================

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

// ===========================================================================
// Board operations
// ===========================================================================

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
          // Write color only for the real board (not test_board)
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
      // Shift color rows too (only for real board, only visible rows)
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

// ===========================================================================
// Heuristics
// ===========================================================================

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

// ===========================================================================
// AI placement
// ===========================================================================

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

// ===========================================================================
// Game operations
// ===========================================================================

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
  int lines;
  piece_col = ai_target_col;
  piece_rot = ai_target_rot;
  piece_row = ai_target_row;
  place_piece(board, current_piece, piece_row, piece_col, piece_rot);
  game_score = game_score + 1;
  lines = clear_lines(board);
  if (lines > 0)
  {
    game_score = game_score + score_for_lines(lines);
    game_lines = game_lines + lines;
  }
}

void init_game(void)
{
  clear_board();
  rng_seed(gen_seed);
  shuffle_bag();
  current_piece = T_NONE;
  next_piece = next_from_bag();
  piece_row = 0;
  piece_col = 0;
  piece_rot = 0;
  game_score = 0;
  game_lines = 0;
  game_pieces = 0;
  game_over = 0;
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

// Send board snapshot to host
void send_board_snapshot(void)
{
  char data[212];
  int row;
  int col;
  int i;

  // Pack 20 visible rows × 10 cols as color values (200 bytes)
  i = 0;
  for (row = 0; row < VISIBLE_ROWS; row++)
  {
    for (col = 0; col < BOARD_COLS; col++)
    {
      data[i] = board_colors[row * BOARD_COLS + col] & 0xFF;
      i = i + 1;
    }
  }
  // Score, lines, pieces (4 bytes each)
  write_u32(data, 200, (unsigned int)game_score);
  write_u32(data, 204, (unsigned int)game_lines);
  write_u32(data, 208, (unsigned int)game_pieces);

  // Fire-and-forget (no ACK needed for board snapshots)
  fnp_send(coord_mac, FNP_TYPE_TETRIS_BOARD, tx_seq, 0,
           data, 212, frame_buf);
  tx_seq = tx_seq + 1;
}

// Send final result to host (reliable)
void send_result(void)
{
  char data[16];
  write_u32(data, 0, (unsigned int)chromo_id);
  write_u32(data, 4, (unsigned int)game_score);
  write_u32(data, 8, (unsigned int)game_lines);
  write_u32(data, 12, (unsigned int)game_pieces);

  fnp_send_reliable(coord_mac, FNP_TYPE_TETRIS_RESULT,
                    data, 16, frame_buf, &tx_seq);
}

// Parse TETRIS_PARAMS: 4 bytes seed
void parse_params(char *data, int data_len)
{
  if (data_len < 4) return;
  gen_seed = read_u32(data, 0);
  has_params = 1;
}

// Parse TETRIS_ASSIGN: 4 bytes chromo_id + 24 bytes weights (6 × Q16.16)
void parse_assign(char *data, int data_len)
{
  if (data_len < 28) return;
  chromo_id      = (int)read_u32(data, 0);
  w_lines        = (int)read_u32(data, 4);
  w_delta_height = (int)read_u32(data, 8);
  w_holes        = (int)read_u32(data, 12);
  w_big_wells    = (int)read_u32(data, 16);
  w_max_hole_dist= (int)read_u32(data, 20);
  w_bumpiness    = (int)read_u32(data, 24);
  has_assign = 1;
}

// Check for incoming network messages (non-blocking).
// Returns 1 if a new assignment was received (game should start/restart).
int poll_network(void)
{
  int rxlen;
  int src_mac[6];
  int msg_type;
  int rx_seq;
  int rx_flags;
  char *rx_data;
  int rx_data_len;
  int got_assign;

  got_assign = 0;

  while (sys_net_packet_count() > 0)
  {
    rxlen = sys_net_recv(frame_buf, FNP_FRAME_BUF_SIZE);
    if (rxlen > 0 && fnp_parse(frame_buf, rxlen, src_mac,
                                &msg_type, &rx_seq, &rx_flags,
                                &rx_data, &rx_data_len))
    {
      // Save coordinator MAC
      {
        int i;
        for (i = 0; i < 6; i++) coord_mac[i] = src_mac[i];
      }

      if (msg_type == FNP_TYPE_TETRIS_PARAMS)
      {
        parse_params(rx_data, rx_data_len);
      }
      else if (msg_type == FNP_TYPE_TETRIS_ASSIGN)
      {
        parse_assign(rx_data, rx_data_len);
        got_assign = 1;
      }
    }
  }
  return got_assign;
}

// ===========================================================================
// Print helpers
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

// ===========================================================================
// Main
// ===========================================================================

int main(void)
{
  int our_mac[6];
  int keys;
  int game_count;

  // Initialize piece data pointers
  piece_data[0] = t_i;
  piece_data[1] = t_j;
  piece_data[2] = t_l;
  piece_data[3] = t_o;
  piece_data[4] = t_s;
  piece_data[5] = t_t;
  piece_data[6] = t_z;

  // Initialize FNP
  fnp_init();
  fnp_get_our_mac(our_mac);
  tx_seq = 0;
  has_params = 0;
  has_assign = 0;
  game_count = 0;

  sys_putstr("Tetris GA Worker ready\n");
  sys_putstr("Waiting for assignments...\n");

  // ---- Main loop ----
  while (1)
  {
    // Check escape
    keys = sys_get_key_state();
    if (keys & KEYSTATE_ESCAPE) break;

    // Wait for params + assign
    if (!has_assign)
    {
      poll_network();
      sys_delay(1);
      continue;
    }

    // Got an assignment — play a game
    has_assign = 0;
    game_count = game_count + 1;

    sys_putstr("Game ");
    print_int(game_count);
    sys_putstr(" (chromo ");
    print_int(chromo_id);
    sys_putstr(")...");

    init_game();
    spawn_piece();

    // Play until game over
    while (!game_over)
    {
      // Check for new params/assign that might interrupt
      if (poll_network())
      {
        // New assignment received — abort current game and restart
        break;
      }

      hard_drop_and_land();

      // Send board snapshot periodically
      if (game_pieces % BOARD_SEND_INTERVAL == 0)
      {
        send_board_snapshot();
      }

      if (!game_over)
      {
        spawn_piece();
      }
    }

    // Game ended (or interrupted)
    if (game_over)
    {
      // Send final board snapshot and result
      send_board_snapshot();
      send_result();

      sys_putstr(" score=");
      print_int(game_score);
      sys_putstr(" lines=");
      print_int(game_lines);
      sys_putstr(" pieces=");
      print_int(game_pieces);
      sys_putstr("\n");
    }
  }

  sys_term_clear();
  return 0;
}
