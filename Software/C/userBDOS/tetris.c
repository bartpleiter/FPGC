//
// tetris.c — Standalone AI Tetris (userBDOS).
// Plays Tetris autonomously using heuristic AI with configurable weights.
// Foundation for the cluster Tetris GA demo.
//

#define USER_SYSCALL
#include "libs/user/user.h"

// ---- Screen constants ----
#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240
#define PIXEL_FB_ADDR 0x7B00000

unsigned int *fb = (unsigned int *)PIXEL_FB_ADDR;

// ---- Board constants ----
#define BOARD_ROWS    22   // 20 visible + 2 hidden spawn rows
#define BOARD_COLS    10
#define VISIBLE_ROWS  20

// Board: each row is an int, low 10 bits = columns (bit 0 = col 0)
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

// ---- Piece data ----
// Each piece stores cell offsets as (row, col) pairs for each rotation.
// Format: piece_cells[piece][rotation][cell_index] = row*16 + col
// Each piece has 4 cells, up to 4 rotations (O has 1).
// Derived from the original bool grid data.

// Number of rotations per piece
int piece_num_rots[7] = {4, 4, 4, 1, 4, 4, 4};

// Piece grid dimensions: rows, cols per piece
int piece_rows[7] = {4, 3, 3, 3, 3, 3, 3};
int piece_cols[7] = {4, 3, 3, 4, 3, 3, 3};

// Piece grid data — flat boolean arrays, same layout as original:
// [rotation * rows * cols + row * cols + col], row 0 = bottom of piece

// I piece (4x4, 4 rotations = 64 entries)
int t_i[64] = {
  0,0,0,0, 0,0,0,0, 1,1,1,1, 0,0,0,0,
  0,0,1,0, 0,0,1,0, 0,0,1,0, 0,0,1,0,
  0,0,0,0, 1,1,1,1, 0,0,0,0, 0,0,0,0,
  0,1,0,0, 0,1,0,0, 0,1,0,0, 0,1,0,0
};

// J piece (3x3, 4 rotations = 36 entries)
int t_j[36] = {
  0,0,0, 1,1,1, 1,0,0,
  0,1,0, 0,1,0, 0,1,1,
  0,0,1, 1,1,1, 0,0,0,
  1,1,0, 0,1,0, 0,1,0
};

// L piece (3x3, 4 rotations = 36 entries)
int t_l[36] = {
  0,0,0, 1,1,1, 0,0,1,
  0,1,1, 0,1,0, 0,1,0,
  1,0,0, 1,1,1, 0,0,0,
  0,1,0, 0,1,0, 1,1,0
};

// O piece (3x4, 1 rotation = 12 entries)
int t_o[12] = {
  0,0,0,0, 0,1,1,0, 0,1,1,0
};

// S piece (3x3, 4 rotations = 36 entries)
int t_s[36] = {
  0,0,0, 1,1,0, 0,1,1,
  0,0,1, 0,1,1, 0,1,0,
  1,1,0, 0,1,1, 0,0,0,
  0,1,0, 1,1,0, 1,0,0
};

// T piece (3x3, 4 rotations = 36 entries)
int t_t[36] = {
  0,0,0, 1,1,1, 0,1,0,
  0,1,0, 0,1,1, 0,1,0,
  0,1,0, 1,1,1, 0,0,0,
  0,1,0, 1,1,0, 0,1,0
};

// Z piece (3x3, 4 rotations = 36 entries)
int t_z[36] = {
  0,0,0, 0,1,1, 1,1,0,
  0,1,0, 0,1,1, 0,0,1,
  0,1,1, 1,1,0, 0,0,0,
  1,0,0, 1,1,0, 0,1,0
};

// Pointers to piece data arrays
int *piece_data[7];

// ---- SRS Wall Kick Data ----
// wallkick_normal: for J/L/S/T/Z — 8 transitions × 5 tests × 2 (col_offset, row_offset)
// Transitions: 0→R, R→0, R→2, 2→R, 2→L, L→2, L→0, 0→L
int wallkick_normal[80] = {
   0, 0,  -1, 0,  -1, 1,   0,-2,  -1,-2,
   0, 0,   1, 0,   1,-1,   0, 2,   1, 2,
   0, 0,   1, 0,   1,-1,   0, 2,   1, 2,
   0, 0,  -1, 0,  -1, 1,   0,-2,  -1,-2,
   0, 0,   1, 0,   1, 1,   0,-2,   1,-2,
   0, 0,  -1, 0,  -1,-1,   0, 2,  -1, 2,
   0, 0,  -1, 0,  -1,-1,   0, 2,  -1, 2,
   0, 0,   1, 0,   1, 1,   0,-2,   1,-2
};

// wallkick_i: for I piece
int wallkick_i[80] = {
   0, 0,  -2, 0,   1, 0,  -2,-1,   1, 2,
   0, 0,   2, 0,  -1, 0,   2, 1,  -1,-2,
   0, 0,  -1, 0,   2, 0,  -1, 2,   2,-1,
   0, 0,   1, 0,  -2, 0,   1,-2,  -2, 1,
   0, 0,   2, 0,  -1, 0,   2, 1,  -1,-2,
   0, 0,  -2, 0,   1, 0,  -2,-1,   1, 2,
   0, 0,   1, 0,  -2, 0,   1,-2,  -2, 1,
   0, 0,  -1, 0,   2, 0,  -1, 2,   2,-1
};

// Map (old_rot, new_rot) to wall kick table index
// 0→1=0, 1→0=1, 1→2=2, 2→1=3, 2→3=4, 3→2=5, 3→0=6, 0→3=7
int get_kick_index(int old_rot, int new_rot)
{
  if (old_rot == 0 && new_rot == 1) return 0;
  if (old_rot == 1 && new_rot == 0) return 1;
  if (old_rot == 1 && new_rot == 2) return 2;
  if (old_rot == 2 && new_rot == 1) return 3;
  if (old_rot == 2 && new_rot == 3) return 4;
  if (old_rot == 3 && new_rot == 2) return 5;
  if (old_rot == 3 && new_rot == 0) return 6;
  if (old_rot == 0 && new_rot == 3) return 7;
  return 0;
}

// ---- Game state ----
int current_piece;
int next_piece;
int piece_row;   // bottom-left origin row
int piece_col;   // bottom-left origin col
int piece_rot;

int game_score;
int game_lines;
int game_pieces;
int game_over;

// ---- AI target ----
int ai_target_row;
int ai_target_col;
int ai_target_rot;

// ---- AI weights (Q16.16 fixed-point) ----
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

void rng_seed(unsigned int s)
{
  rng_state = s;
  if (rng_state == 0) rng_state = 1;
}

unsigned int rng_next()
{
  // LCG: x = x * 1103515245 + 12345
  rng_state = rng_state * 1103515245 + 12345;
  return rng_state;
}

void shuffle_bag()
{
  int i;
  int j;
  int tmp;

  // Initialize bag with 0–6
  for (i = 0; i < 7; i++)
  {
    bag[i] = i;
  }
  // Fisher-Yates shuffle
  for (i = 6; i > 0; i--)
  {
    j = rng_next() % (i + 1);
    tmp = bag[i];
    bag[i] = bag[j];
    bag[j] = tmp;
  }
  bag_index = 0;
}

int next_from_bag()
{
  int p;
  p = bag[bag_index];
  bag_index = bag_index + 1;
  if (bag_index >= 7)
  {
    shuffle_bag();
  }
  return p;
}

// ---- Board operations ----

void clear_board()
{
  int i;
  for (i = 0; i < BOARD_ROWS; i++)
  {
    board[i] = 0;
  }
}

// Check if a piece cell is set in piece data
int piece_cell(int piece, int rot, int r, int c)
{
  int rows;
  int cols;
  int *data;
  int idx;

  rows = piece_rows[piece];
  cols = piece_cols[piece];
  data = piece_data[piece];

  idx = rot * rows * cols + r * cols + c;
  return data[idx];
}

// Check if piece at (row, col, rot) collides with board or boundaries
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

        // Out of bounds
        if (mc < 0 || mc >= BOARD_COLS || mr < 0)
        {
          return 1;
        }
        // Above board is OK (spawn area)
        if (mr >= BOARD_ROWS)
        {
          continue;
        }
        // Overlaps existing block
        if (brd[mr] & (1 << mc))
        {
          return 1;
        }
      }
    }
  }
  return 0;
}

// Place piece on board
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
        }
      }
    }
  }
}

// Clear filled rows, shift above rows down, return number of lines cleared
int clear_lines(int *brd)
{
  int row;
  int dst;
  int cleared;

  cleared = 0;
  dst = 0;

  for (row = 0; row < BOARD_ROWS; row++)
  {
    if ((brd[row] & 0x3FF) == 0x3FF)
    {
      // Full row — skip it (don't copy to dst)
      cleared = cleared + 1;
    }
    else
    {
      brd[dst] = brd[row];
      dst = dst + 1;
    }
  }
  // Fill the vacated top rows with empty
  while (dst < BOARD_ROWS)
  {
    brd[dst] = 0;
    dst = dst + 1;
  }

  return cleared;
}

// ---- Scoring ----
int score_for_lines(int n)
{
  if (n == 1) return 40;
  if (n == 2) return 100;
  if (n == 3) return 300;
  if (n == 4) return 1200;
  return 0;
}

// ---- Column heights (helper for heuristics) ----
// heights: array of 10 ints, filled with column heights (0 = empty, max 22)
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

// ---- Heuristic functions ----

// Count full rows (before clearing)
int h_lines_cleared(int *brd)
{
  int row;
  int count;

  count = 0;
  for (row = 0; row < BOARD_ROWS; row++)
  {
    if ((brd[row] & 0x3FF) == 0x3FF)
    {
      count = count + 1;
    }
  }
  return count;
}

// Max column height minus min column height
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

// Count holes: empty cells with at least one filled cell above in same column
int h_holes(int *brd, int *heights)
{
  int col;
  int row;
  int count;

  count = 0;
  for (col = 0; col < BOARD_COLS; col++)
  {
    // Only scan below the column height
    for (row = 0; row < heights[col]; row++)
    {
      if (!(brd[row] & (1 << col)))
      {
        count = count + 1;
      }
    }
  }
  return count;
}

// Count big wells: columns where both neighbors are ≥2 higher
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

    if (lh > heights[col] + 1 && rh > heights[col] + 1)
    {
      count = count + 1;
    }
  }
  return count;
}

// Horizontal distance between leftmost and rightmost columns with holes
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

// Sum of absolute height differences between adjacent columns
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

// ---- AI: evaluate board with weights (Q16.16 × int = Q16.16) ----
int evaluate_board(int *brd)
{
  int heights[10];
  int score;

  compute_col_heights(brd, heights);

  score = w_lines * h_lines_cleared(brd) +
          w_delta_height * h_height_diff(heights) +
          w_holes * h_holes(brd, heights) +
          w_big_wells * h_big_wells(heights) +
          w_max_hole_dist * h_max_hole_dist(brd, heights) +
          w_bumpiness * h_bumpiness(heights);

  return score;
}

// ---- AI: find best placement for a piece ----
// Temporary board for evaluating placements
int test_board[BOARD_ROWS];

#define MAX_POSITIONS 60

int pos_row[MAX_POSITIONS];
int pos_col[MAX_POSITIONS];
int pos_rot[MAX_POSITIONS];
int num_positions;

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

        // Piece fits here AND doesn't fit one row below (sitting on something)
        if (!collides(brd, piece, row, col, rot) &&
            collides(brd, piece, row - 1, col, rot))
        {
          // Check reachability: piece can fall from above without obstruction
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

  best_score = 0x80000000; // INT_MIN
  best_idx = 0;

  for (i = 0; i < num_positions; i++)
  {
    // Copy board
    for (r = 0; r < BOARD_ROWS; r++)
    {
      test_board[r] = brd[r];
    }

    // Place piece and evaluate
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
    // No valid placements — game will end
    ai_target_row = piece_row;
    ai_target_col = piece_col;
    ai_target_rot = piece_rot;
  }
}

// ---- Spawn a new piece ----
void spawn_piece()
{
  current_piece = next_piece;
  next_piece = next_from_bag();

  // Spawn position: row 17, col 3 (standard Tetris spawn)
  piece_row = 17;
  piece_col = 3;
  piece_rot = 0;

  game_pieces = game_pieces + 1;

  // Check if spawn position is blocked
  if (collides(board, current_piece, piece_row, piece_col, piece_rot))
  {
    game_over = 1;
    return;
  }

  // Calculate AI target for this piece
  calculate_best_placement(board, current_piece);
}

// ---- Hard drop: drop piece to target and land it ----
void hard_drop_and_land()
{
  // Move piece to AI target position
  piece_col = ai_target_col;
  piece_rot = ai_target_rot;
  piece_row = ai_target_row;

  // Place piece on board
  place_piece(board, current_piece, piece_row, piece_col, piece_rot);

  // Score: +1 for placing a piece
  game_score = game_score + 1;

  // Clear lines
  {
    int lines;
    lines = clear_lines(board);
    if (lines > 0)
    {
      game_score = game_score + score_for_lines(lines);
      game_lines = game_lines + lines;
    }
  }
}

// ---- Initialize a new game ----
void init_game(unsigned int seed)
{
  clear_board();
  rng_seed(seed);
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

// ---- Display ----

// Board display position on pixel framebuffer
#define DISP_BOARD_X  110
#define DISP_BOARD_Y  20
#define CELL_SIZE     5

// Piece colors (palette indices — simple distinct colors)
int piece_colors[7] = {
  0x03,  // I = cyan-ish
  0x02,  // J = blue-ish
  0xE0,  // L = orange
  0xFC,  // O = yellow
  0x1C,  // S = green
  0xE3,  // T = purple
  0xC0   // Z = red
};

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
          fb[py * SCREEN_WIDTH + px] = color;
        }
      }
    }
  }
}

void draw_board()
{
  int row;
  int col;
  int x;
  int y;
  int color;

  // Draw border
  draw_rect(DISP_BOARD_X - 2, DISP_BOARD_Y - 2,
            BOARD_COLS * CELL_SIZE + 4, VISIBLE_ROWS * CELL_SIZE + 4, 0xFF);

  // Draw board background
  draw_rect(DISP_BOARD_X, DISP_BOARD_Y,
            BOARD_COLS * CELL_SIZE, VISIBLE_ROWS * CELL_SIZE, 0x00);

  // Draw placed blocks
  for (row = 0; row < VISIBLE_ROWS; row++)
  {
    for (col = 0; col < BOARD_COLS; col++)
    {
      if (board[row] & (1 << col))
      {
        x = DISP_BOARD_X + col * CELL_SIZE;
        y = DISP_BOARD_Y + (VISIBLE_ROWS - 1 - row) * CELL_SIZE;
        // Gray for placed blocks (we don't track which piece placed them)
        draw_rect(x, y, CELL_SIZE - 1, CELL_SIZE - 1, 0xB6);
      }
    }
  }
}

// ---- Print helpers ----
void print_int(int val)
{
  char buf[12];
  int i;
  int neg;
  unsigned int uval;

  if (val == 0)
  {
    sys_print_char('0');
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

  sys_print_str(buf + i);
}

// ---- Status display via terminal ----
void draw_status()
{
  sys_term_set_cursor(0, 0);
  sys_print_str("Tetris AI\n\n");
  sys_print_str("Score:  ");
  print_int(game_score);
  sys_print_str("\nLines:  ");
  print_int(game_lines);
  sys_print_str("\nPieces: ");
  print_int(game_pieces);
  sys_print_str("\n");
}

// ---- Main ----

int main()
{
  int running;
  int keys;
  int frame_count;

  // Initialize piece data pointers
  piece_data[0] = t_i;
  piece_data[1] = t_j;
  piece_data[2] = t_l;
  piece_data[3] = t_o;
  piece_data[4] = t_s;
  piece_data[5] = t_t;
  piece_data[6] = t_z;

  // Set default AI weights (Q16.16):
  // wLines=3.0, wDeltaHeight=-1.0, wHoles=-3.0,
  // wBigWells=-2.0, wMaxHoleDist=-1.0, wBumpiness=-1.0
  w_lines        =  3 << 16;  //  3.0
  w_delta_height = -(1 << 16); // -1.0
  w_holes        = -(3 << 16); // -3.0
  w_big_wells    = -(2 << 16); // -2.0
  w_max_hole_dist= -(1 << 16); // -1.0
  w_bumpiness    = -(1 << 16); // -1.0

  // Clear screen
  sys_term_clear();
  {
    int ci;
    for (ci = 0; ci < SCREEN_WIDTH * SCREEN_HEIGHT; ci++)
    {
      fb[ci] = 0;
    }
  }

  // Initialize game
  init_game(42);
  spawn_piece();

  frame_count = 0;
  running = 1;

  while (running)
  {
    // Check escape
    keys = sys_get_key_state();
    if (keys & KEYSTATE_ESCAPE)
    {
      running = 0;
      break;
    }

    if (game_over)
    {
      draw_board();
      draw_status();
      sys_print_str("\nGAME OVER!\n");
      sys_print_str("Final score: ");
      print_int(game_score);
      sys_print_str("\nLines: ");
      print_int(game_lines);
      sys_print_str("\nPieces: ");
      print_int(game_pieces);
      sys_print_str("\n\nPress Escape to exit");

      // Wait for escape
      while (1)
      {
        keys = sys_get_key_state();
        if (keys & KEYSTATE_ESCAPE) break;
        sys_delay(50);
      }
      running = 0;
      break;
    }

    // AI plays: hard drop to target, then spawn next piece
    hard_drop_and_land();
    spawn_piece();

    // Update display every 10 pieces to keep it fast
    frame_count = frame_count + 1;
    if (frame_count % 10 == 0)
    {
      draw_board();
      draw_status();
    }
  }

  // Cleanup: clear pixel framebuffer
  {
    int ci;
    for (ci = 0; ci < SCREEN_WIDTH * SCREEN_HEIGHT; ci++)
    {
      fb[ci] = 0;
    }
  }
  sys_term_clear();

  return 0;
}
