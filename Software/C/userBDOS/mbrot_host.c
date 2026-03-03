//
// mbrot_host.c — Cluster Mandelbrot coordinator (userBDOS).
// Controls 4 worker FPGCs (MAC :02–:05) for a Mandelbrot zoom animation.
//

#define USER_SYSCALL
#define USER_FNP
#include "libs/user/user.h"

// ---- Screen constants ----
#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240
#define PIXEL_FB_ADDR 0x7B00000

unsigned int *fb = (unsigned int *)PIXEL_FB_ADDR;

// ---- Off-screen buffer ----
// Workers write results here; upscale reads from here and writes to fb.
int backbuf[76800];  // 320 * 240

// Upscale source rect for 0.90× zoom (center 90% of image)
// 320 * 0.9 = 288, (320 - 288) / 2 = 16
// 240 * 0.9 = 216, (240 - 216) / 2 = 12
#define UPSCALE_X0  16
#define UPSCALE_Y0  12
#define UPSCALE_W   288
#define UPSCALE_H   216


// ---- Worker configuration ----
#define NUM_WORKERS   4
#define ROWS_PER_WORKER (SCREEN_HEIGHT / NUM_WORKERS)  // 60

// Worker MAC addresses — flat array to avoid 2D array pointer issues in B32CC.
// Layout: 6 ints per worker, 4 workers = 24 ints.
// Worker 0 = MAC :02, Worker 1 = MAC :03, Worker 2 = MAC :04, Worker 3 = MAC :05
int worker_mac_data[24] = {
  0x02, 0xB4, 0xB4, 0x00, 0x00, 0x02,
  0x02, 0xB4, 0xB4, 0x00, 0x00, 0x03,
  0x02, 0xB4, 0xB4, 0x00, 0x00, 0x04,
  0x02, 0xB4, 0xB4, 0x00, 0x00, 0x05
};

// Helper: get pointer to worker w's MAC (6-element int array)
int *get_worker_mac(int w)
{
  return &worker_mac_data[w * 6];
}

// ---- Chunk tracking ----
// Each worker sends ceil(60*320/1020) = 19 chunks
#define PIXELS_PER_WORKER  (ROWS_PER_WORKER * SCREEN_WIDTH)  // 19200
#define CHUNK_MAX_PIXELS   1020
#define CHUNKS_PER_WORKER  19  // ceil(19200/1020)
#define TOTAL_CHUNKS       (CHUNKS_PER_WORKER * NUM_WORKERS)  // 76
#define CHUNK_HEADER_SIZE  4

// Track which chunks have been received
int chunks_received[TOTAL_CHUNKS];
int total_chunks_done;

// ---- Protocol frame buffer ----
char frame_buf[FNP_FRAME_BUF_SIZE];

// ---- Sequence counter ----
int tx_seq;

// ---- Palette data (256 24-bit RGB entries) ----
#define NUM_PALETTES 5
int current_palette;

// Palette storage: 256 entries of 24-bit RGB
int palette[256];

// ---- View state (Q32.32 fixed-point) ----
int center_re_hi;
unsigned int center_re_lo;
int center_im_hi;
unsigned int center_im_lo;
int scale_hi;
unsigned int scale_lo;
int max_iter_val;

// Minimum scale before reset (~1e-7 in Q32.32)
#define MIN_SCALE_LO 0x00001000

// Zoom factor per frame: 0.90 in Q32.32
#define ZOOM_FACTOR_LO 0xE6666666

// ---- Auto-zoom state ----
int auto_zoom;

// ---- Collection timeout (ms) ----
#define COLLECT_TIMEOUT_MS 3000

// ---- Helper: write big-endian 32-bit into buffer ----
void write_u32(char *buf, int offset, unsigned int val)
{
  buf[offset]     = (val >> 24) & 0xFF;
  buf[offset + 1] = (val >> 16) & 0xFF;
  buf[offset + 2] = (val >> 8) & 0xFF;
  buf[offset + 3] = val & 0xFF;
}

// ---- Helper: read big-endian 32-bit from buffer ----
unsigned int read_u32(char *buf, int offset)
{
  return ((buf[offset] & 0xFF) << 24) |
         ((buf[offset + 1] & 0xFF) << 16) |
         ((buf[offset + 2] & 0xFF) << 8) |
         (buf[offset + 3] & 0xFF);
}

// ---- Palette generation ----

// Interpolate between two RGB colors at position t (0-255)
int lerp_color(int c0, int c1, int t)
{
  int r0;
  int g0;
  int b0;
  int r1;
  int g1;
  int b1;
  int r;
  int g;
  int b;

  r0 = (c0 >> 16) & 0xFF;
  g0 = (c0 >> 8) & 0xFF;
  b0 = c0 & 0xFF;
  r1 = (c1 >> 16) & 0xFF;
  g1 = (c1 >> 8) & 0xFF;
  b1 = c1 & 0xFF;

  r = r0 + ((r1 - r0) * t) / 255;
  g = g0 + ((g1 - g0) * t) / 255;
  b = b0 + ((b1 - b0) * t) / 255;

  return (r << 16) | (g << 8) | b;
}

// Build a gradient palette from an array of color stops.
// stops: array of {position (0-255), RGB color} pairs
// num_stops: number of stops
void build_gradient(int *stops_pos, int *stops_color, int num_stops)
{
  int i;
  int seg;

  palette[0] = 0x000000; // Index 0 = always black (in set)

  for (i = 1; i < 256; i++)
  {
    // Find which segment this index falls in
    seg = 0;
    while (seg < num_stops - 2 && i > stops_pos[seg + 1])
    {
      seg = seg + 1;
    }

    // Interpolate within segment
    {
      int seg_start = stops_pos[seg];
      int seg_end = stops_pos[seg + 1];
      int seg_len = seg_end - seg_start;
      int t;
      if (seg_len > 0)
      {
        t = ((i - seg_start) * 255) / seg_len;
      }
      else
      {
        t = 0;
      }
      palette[i] = lerp_color(stops_color[seg], stops_color[seg + 1], t);
    }
  }
}

void load_palette_classic()
{
  // Classic: Blue → Cyan → Green → Yellow → Red → Black
  int pos[6]   = {1,   51,  102,  153,  204,  255};
  int col[6] = {0x000080, 0x00FFFF, 0x00FF00, 0xFFFF00, 0xFF0000, 0x000000};
  build_gradient(pos, col, 6);
}

void load_palette_fire()
{
  // Fire: Black → Dark Red → Orange → Yellow → White
  int pos[5]   = {1,   64,  128,  192,  255};
  int col[5] = {0x200000, 0xAA0000, 0xFF6600, 0xFFFF00, 0xFFFFFF};
  build_gradient(pos, col, 5);
}

void load_palette_ice()
{
  // Ice: Black → Deep Blue → Cyan → White
  int pos[4]   = {1,   85,  170,  255};
  int col[4] = {0x000020, 0x0000CC, 0x00CCFF, 0xFFFFFF};
  build_gradient(pos, col, 4);
}

void load_palette_ultra()
{
  // Ultra Fractal style: smooth HSV-like rotation
  int pos[7]   = {1,   42,  85,  128,  170,  213,  255};
  int col[7] = {0x000764, 0x206BCB, 0xEDFFFF, 0xFFAA00, 0x310230, 0x000764, 0x000764};
  build_gradient(pos, col, 7);
}

void load_palette_mono()
{
  // Monochrome: Black → White linear
  int i;
  palette[0] = 0x000000;
  for (i = 1; i < 256; i++)
  {
    int v = i;
    palette[i] = (v << 16) | (v << 8) | v;
  }
}

void load_palette(int index)
{
  switch (index)
  {
    case 0: load_palette_classic(); break;
    case 1: load_palette_fire(); break;
    case 2: load_palette_ice(); break;
    case 3: load_palette_ultra(); break;
    case 4: load_palette_mono(); break;
    default: load_palette_classic(); break;
  }
}

// Write current palette array to GPU hardware
void apply_palette()
{
  int i;
  for (i = 0; i < 256; i++)
  {
    sys_set_pixel_palette(i, palette[i]);
  }
}

// ---- Identify worker from source MAC ----
// Returns worker index (0-3) or -1 if unknown.
// Uses source MAC's last byte: 0x02=worker0, 0x03=worker1, etc.
int identify_worker(int *src_mac)
{
  int last_byte;
  last_byte = src_mac[5] & 0xFF;
  if (last_byte >= 0x02 && last_byte <= 0x05)
  {
    // Verify the prefix bytes match
    if (src_mac[0] == 0x02 && src_mac[1] == 0xB4 && src_mac[2] == 0xB4 &&
        src_mac[3] == 0x00 && src_mac[4] == 0x00)
    {
      return last_byte - 0x02;
    }
  }
  return -1;
}

// ---- Send CLUSTER_PARAMS to all workers ----
void send_params()
{
  char payload[28];
  int w;

  write_u32(payload, 0, (unsigned int)center_re_hi);
  write_u32(payload, 4, center_re_lo);
  write_u32(payload, 8, (unsigned int)center_im_hi);
  write_u32(payload, 12, center_im_lo);
  write_u32(payload, 16, (unsigned int)scale_hi);
  write_u32(payload, 20, scale_lo);
  write_u32(payload, 24, (unsigned int)max_iter_val);

  for (w = 0; w < NUM_WORKERS; w++)
  {
    fnp_send(get_worker_mac(w), FNP_TYPE_CLUSTER_PARAMS, tx_seq, 0,
             payload, 28, frame_buf);
    tx_seq = tx_seq + 1;
  }
}

// ---- Send CLUSTER_ASSIGN to each worker ----
void send_assignments()
{
  char payload[12];
  int w;
  int y_start;
  int y_end;

  for (w = 0; w < NUM_WORKERS; w++)
  {
    y_start = w * ROWS_PER_WORKER;
    y_end = y_start + ROWS_PER_WORKER;
    if (y_end > SCREEN_HEIGHT)
    {
      y_end = SCREEN_HEIGHT;
    }

    // Include worker_id so workers can verify the assignment is for them
    write_u32(payload, 0, (unsigned int)w);
    write_u32(payload, 4, (unsigned int)y_start);
    write_u32(payload, 8, (unsigned int)y_end);

    fnp_send(get_worker_mac(w), FNP_TYPE_CLUSTER_ASSIGN, tx_seq, 0,
             payload, 12, frame_buf);
    tx_seq = tx_seq + 1;
  }
}

// ---- Process a received CLUSTER_RESULT chunk ----
// Returns 1 if a new chunk was successfully processed, 0 otherwise.
int process_result_chunk(int worker_id, char *data, int data_len)
{
  unsigned int chunk_index;
  int pixel_count;
  int global_chunk;
  int pixel_offset;
  int i;

  if (data_len < CHUNK_HEADER_SIZE)
    return 0;

  chunk_index = read_u32(data, 0);
  pixel_count = data_len - CHUNK_HEADER_SIZE;

  // Compute global chunk index
  global_chunk = worker_id * CHUNKS_PER_WORKER + (int)chunk_index;
  if (global_chunk < 0 || global_chunk >= TOTAL_CHUNKS)
    return 0;

  // Skip if already received
  if (chunks_received[global_chunk])
    return 0;

  // Compute pixel offset in framebuffer
  // Worker's pixel area starts at worker_id * PIXELS_PER_WORKER
  // Chunk within worker starts at chunk_index * CHUNK_MAX_PIXELS
  pixel_offset = worker_id * PIXELS_PER_WORKER + (int)chunk_index * CHUNK_MAX_PIXELS;

  // Write pixels to both off-screen buffer and framebuffer.
  // Writing to fb directly gives smooth progressive fill over
  // the upscaled zoom preview.
  i = 0;
  while (i < pixel_count && (pixel_offset + i) < (SCREEN_WIDTH * SCREEN_HEIGHT))
  {
    int px;
    px = data[CHUNK_HEADER_SIZE + i] & 0xFF;
    backbuf[pixel_offset + i] = px;
    fb[pixel_offset + i] = px;
    i = i + 1;
  }

  chunks_received[global_chunk] = 1;
  total_chunks_done = total_chunks_done + 1;

  return 1;
}

// ---- Blit backbuf directly to pixel framebuffer ----
void blit_backbuf()
{
  int i;
  for (i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++)
  {
    fb[i] = backbuf[i];
  }
}

// ---- Upscale center of backbuf → pixel framebuffer (fused) ----
// Nearest-neighbor scales UPSCALE_W × UPSCALE_H region of backbuf
// (starting at UPSCALE_X0, UPSCALE_Y0) directly to fb.
// Eliminates prevbuf by writing to fb instead of backbuf.
void upscale_to_fb()
{
  int ox;
  int oy;
  int sx;
  int sy;

  for (oy = 0; oy < SCREEN_HEIGHT; oy++)
  {
    sy = UPSCALE_Y0 + (oy * UPSCALE_H) / SCREEN_HEIGHT;
    for (ox = 0; ox < SCREEN_WIDTH; ox++)
    {
      sx = UPSCALE_X0 + (ox * UPSCALE_W) / SCREEN_WIDTH;
      fb[oy * SCREEN_WIDTH + ox] = backbuf[sy * SCREEN_WIDTH + sx];
    }
  }
}

// ---- Reset chunk collection tracking ----
void reset_collection()
{
  int i;
  for (i = 0; i < TOTAL_CHUNKS; i++)
  {
    chunks_received[i] = 0;
  }
  total_chunks_done = 0;
}

// ---- Try to process all available network packets (non-blocking) ----
// Returns number of new chunks received.
int try_collect_packets()
{
  int rxlen;
  int src_mac[6];
  int msg_type;
  int rx_seq;
  int rx_flags;
  char *rx_data;
  int rx_data_len;
  int worker_id;
  int collected;

  collected = 0;
  while (sys_net_packet_count() > 0)
  {
    rxlen = sys_net_recv(frame_buf, FNP_FRAME_BUF_SIZE);
    if (rxlen > 0 && fnp_parse(frame_buf, rxlen, src_mac,
                                &msg_type, &rx_seq, &rx_flags,
                                &rx_data, &rx_data_len))
    {
      if (msg_type == FNP_TYPE_CLUSTER_RESULT)
      {
        worker_id = identify_worker(src_mac);
        if (worker_id >= 0)
        {
          collected = collected + process_result_chunk(worker_id, rx_data, rx_data_len);
        }
      }
    }
  }
  return collected;
}

// ---- Collect results from workers ----
// Drain packets until all chunks received or timeout.
// process_result_chunk writes to both backbuf and fb, so the
// user sees detail filling in over the zoomed preview in real time.
// Returns 1 if all chunks received, 0 if timed out.
int collect_all(int timeout_ms)
{
  int elapsed;

  elapsed = 0;

  while (total_chunks_done < TOTAL_CHUNKS && elapsed < timeout_ms)
  {
    if (sys_net_packet_count() > 0)
    {
      try_collect_packets();
    }
    else
    {
      sys_delay(5);
      elapsed = elapsed + 5;
    }
  }

  return (total_chunks_done >= TOTAL_CHUNKS) ? 1 : 0;
}

// ---- Compute max iterations from current zoom scale ----
// Uses threshold brackets on scale_lo (when scale_hi == 0)
// to ramp iterations smoothly from 60 (zoomed out) to 400 (deep zoom).
void update_max_iter()
{
  int iter;

  if (scale_hi > 0)
  {
    // Still zoomed out (scale >= 1.0), keep low iteration count
    max_iter_val = 60;
    return;
  }

  // scale_hi == 0: scale is purely fractional (< 1.0)
  // Each threshold is ~4x deeper in zoom, adding ~30-40 iterations
  if      (scale_lo > 0x40000000) iter = 60;  // > 0.25
  else if (scale_lo > 0x10000000) iter = 90;  // > 0.0625
  else if (scale_lo > 0x04000000) iter = 120;  // > 0.015
  else if (scale_lo > 0x01000000) iter = 160;  // > 0.004
  else if (scale_lo > 0x00400000) iter = 200;  // > 0.001
  else if (scale_lo > 0x00100000) iter = 230;  // > 0.00024
  else if (scale_lo > 0x00040000) iter = 260;  // > 0.00006
  else if (scale_lo > 0x00010000) iter = 290;  // > 0.000015
  else if (scale_lo > 0x00004000) iter = 330;  // > 0.000004
  else if (scale_lo > 0x00001000) iter = 370;  // > 0.000001
  else                            iter = 400;

  max_iter_val = iter;
}

// ---- Reset view to initial state ----
void reset_view()
{
  // center = Seahorse Valley (zoom target)
  // Real: -0.743643887... → {-1, 0x41A1BEB0}
  // Imag:  0.131825904... → {0, 0x21C01C90}
  center_re_hi = -1;
  center_re_lo = 0x41A1BEB0;
  center_im_hi = 0;
  center_im_lo = 0x21C01C90;

  // scale = 3.5
  scale_hi = 3;
  scale_lo = 0x80000000;

  update_max_iter();
}

// ---- Initialize view state ----
void init_view()
{
  reset_view();
  auto_zoom = 1;
}

int main()
{
  int our_mac[6];
  int key;
  int keys;
  int running;

  // Initialize
  fnp_init();
  fnp_get_our_mac(our_mac);
  tx_seq = 0;
  current_palette = 3; // Start with Ultra Fractal palette

  // Clear terminal, pixel framebuffer, and backbuf
  sys_term_clear();
  {
    int ci;
    for (ci = 0; ci < SCREEN_WIDTH * SCREEN_HEIGHT; ci++)
    {
      fb[ci] = 0;
      backbuf[ci] = 0;
    }
  }

  // Set up initial view
  init_view();

  // Load and apply palette
  load_palette(current_palette);
  apply_palette();

  // ---- First frame: render without zoom preview ----
  send_params();
  send_assignments();
  reset_collection();
  collect_all(COLLECT_TIMEOUT_MS);
  blit_backbuf();

  // ---- Main render loop ----
  running = 1;
  while (running)
  {
    // Upscale center of completed frame → pixelated zoom preview
    upscale_to_fb();

    // Workers' process_result_chunk writes to both backbuf and fb,
    // so the zoomed preview gets progressively overwritten with detail.

    // Check keyboard input
    while (sys_key_available())
    {
      key = sys_read_key();
      if (key == ' ')
      {
        auto_zoom = !auto_zoom;
      }
      else if (key == 'r' || key == 'R')
      {
        reset_view();
      }
      else if (key == 'p' || key == 'P')
      {
        current_palette = (current_palette + 1) % NUM_PALETTES;
        load_palette(current_palette);
        apply_palette();
      }
    }

    // Check escape key
    keys = sys_get_key_state();
    if (keys & KEYSTATE_ESCAPE)
    {
      running = 0;
      break;
    }

    // Advance zoom if auto-zoom is on
    if (auto_zoom)
    {
      // Zoom in: scale *= 0.90
      __fld(6, scale_hi, scale_lo);
      __fld(7, 0, ZOOM_FACTOR_LO);
      __fmul(6, 6, 7);
      scale_hi = __fsthi(6);
      scale_lo = __fstlo(6);

      // Update iteration count based on zoom depth
      update_max_iter();

      // Check zoom limit
      if (scale_hi == 0 && scale_lo < MIN_SCALE_LO)
      {
        sys_delay(3000);
        reset_view();
      }

      // Dispatch work to workers
      send_params();
      send_assignments();
      reset_collection();

      // Collect results from workers (writes to fb progressively)
      collect_all(COLLECT_TIMEOUT_MS);
    }
    else
    {
      // Paused: idle briefly to avoid busy-spin
      sys_delay(50);
    }
  }

  // Cleanup: restore default RRRGGGBB palette and clear pixel framebuffer
  {
    int i;
    for (i = 0; i < 256; i++)
    {
      int r3;
      int g3;
      int b2;
      int r;
      int g;
      int b;

      r3 = (i >> 5) & 7;
      g3 = (i >> 2) & 7;
      b2 = i & 3;

      r = (r3 << 5) | (r3 << 2) | (r3 >> 1);
      g = (g3 << 5) | (g3 << 2) | (g3 >> 1);
      b = (b2 << 6) | (b2 << 4) | (b2 << 2) | b2;

      sys_set_pixel_palette(i, (r << 16) | (g << 8) | b);
    }
  }
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
