/*
 * mbroth.c — Cluster Mandelbrot coordinator (userBDOS).
 */

//
// mbroth.c — Cluster Mandelbrot coordinator (userBDOS).
// Controls 4 worker FPGCs (MAC :02-:05) for a Mandelbrot zoom animation.
//

#include <syscall.h>
#include <plot.h>
#include <fnp.h>
#include <fixed64.h>

// ---- ANSI / VFS shims for retired syscalls ----
// /dev/pixpal supplies the 8-bit palette DAC; raw /dev/tty supplies
// non-blocking key events. Both fds are opened in main().

static int g_pixpal_fd = -1;
static int g_tty_fd    = -1;

static void term_clear(void)
{
  sys_write(1, "\x1b[2J\x1b[H", 7);
}

static void pixpal_load_all(const int *entries)
{
  if (g_pixpal_fd < 0) return;
  sys_lseek(g_pixpal_fd, 0, SEEK_SET);
  sys_write(g_pixpal_fd, entries, 256 * 4);
}

// ---- Screen constants ----
#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240

// ---- Off-screen buffer ----
int *backbuf;

// Upscale source rect for 0.90x zoom (center 90% of image)
#define UPSCALE_X0  16
#define UPSCALE_Y0  12
#define UPSCALE_W   288
#define UPSCALE_H   216

// ---- Worker configuration ----
#define NUM_WORKERS   4
#define ROWS_PER_WORKER (SCREEN_HEIGHT / NUM_WORKERS)  // 60

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

// ---- Chunk tracking ----
#define PIXELS_PER_WORKER  (ROWS_PER_WORKER * SCREEN_WIDTH)
#define CHUNK_MAX_PIXELS   1020
#define CHUNKS_PER_WORKER  19
#define TOTAL_CHUNKS       (CHUNKS_PER_WORKER * NUM_WORKERS)
#define CHUNK_HEADER_SIZE  4

int chunks_received[TOTAL_CHUNKS];
int total_chunks_done;

// ---- Protocol frame buffer ----
char frame_buf[FNP_FRAME_BUF_SIZE];

// ---- Sequence counter ----
int tx_seq;

// ---- ACK buffer ----
char ack_data[2];

// ---- Palette data ----
#define NUM_PALETTES 5
int current_palette;
int palette[256];

// ---- View state (Q32.32 fixed-point) ----
struct fp64 center_re;
struct fp64 center_im;
struct fp64 scale;
int max_iter_val;

#define MIN_SCALE_LO 0x00001000
#define ZOOM_FACTOR_LO 0xE6666666

int auto_zoom;

#define COLLECT_TIMEOUT_MS 3000

// ---- Helpers ----

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

// ---- Palette generation ----

int lerp_color(int c0, int c1, int t)
{
  int r0, g0, b0, r1, g1, b1, r, g, b;

  r0 = (c0 >> 16) & 0xFF; g0 = (c0 >> 8) & 0xFF; b0 = c0 & 0xFF;
  r1 = (c1 >> 16) & 0xFF; g1 = (c1 >> 8) & 0xFF; b1 = c1 & 0xFF;

  r = r0 + ((r1 - r0) * t) / 255;
  g = g0 + ((g1 - g0) * t) / 255;
  b = b0 + ((b1 - b0) * t) / 255;

  return (r << 16) | (g << 8) | b;
}

void build_gradient(int *stops_pos, int *stops_color, int num_stops)
{
  int i, seg;

  palette[0] = 0x000000;

  for (i = 1; i < 256; i++)
  {
    seg = 0;
    while (seg < num_stops - 2 && i > stops_pos[seg + 1])
      seg = seg + 1;

    {
      int seg_start = stops_pos[seg];
      int seg_end = stops_pos[seg + 1];
      int seg_len = seg_end - seg_start;
      int t;
      if (seg_len > 0)
        t = ((i - seg_start) * 255) / seg_len;
      else
        t = 0;
      palette[i] = lerp_color(stops_color[seg], stops_color[seg + 1], t);
    }
  }
}

void load_palette_classic(void)
{
  int pos[6] = {1, 51, 102, 153, 204, 255};
  int col[6] = {0x000080, 0x00FFFF, 0x00FF00, 0xFFFF00, 0xFF0000, 0x000000};
  build_gradient(pos, col, 6);
}

void load_palette_fire(void)
{
  int pos[5] = {1, 64, 128, 192, 255};
  int col[5] = {0x200000, 0xAA0000, 0xFF6600, 0xFFFF00, 0xFFFFFF};
  build_gradient(pos, col, 5);
}

void load_palette_ice(void)
{
  int pos[4] = {1, 85, 170, 255};
  int col[4] = {0x000020, 0x0000CC, 0x00CCFF, 0xFFFFFF};
  build_gradient(pos, col, 4);
}

void load_palette_ultra(void)
{
  int pos[7] = {1, 42, 85, 128, 170, 213, 255};
  int col[7] = {0x000764, 0x206BCB, 0xEDFFFF, 0xFFAA00, 0x310230, 0x000764, 0x000764};
  build_gradient(pos, col, 7);
}

void load_palette_mono(void)
{
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

void apply_palette(void)
{
  pixpal_load_all(palette);
}

// ---- Worker management ----

void launch_workers(void)
{
  int w;
  for (w = 0; w < NUM_WORKERS; w++)
  {
    fnp_send_command(get_worker_mac(w), "mbrotc", frame_buf, &tx_seq);
  }
  sys_delay(1000);
}

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

void dispatch_workers(void)
{
  char params[28];
  char assign[12];
  int w;
  int y_start;
  int y_end;

  write_u32(params, 0, (unsigned int)center_re.hi);
  write_u32(params, 4, center_re.lo);
  write_u32(params, 8, (unsigned int)center_im.hi);
  write_u32(params, 12, center_im.lo);
  write_u32(params, 16, (unsigned int)scale.hi);
  write_u32(params, 20, scale.lo);
  write_u32(params, 24, (unsigned int)max_iter_val);

  for (w = 0; w < NUM_WORKERS; w++)
  {
    fnp_send(get_worker_mac(w), FNP_TYPE_CLUSTER_PARAMS, tx_seq, 0,
             params, 28, frame_buf);
    tx_seq = tx_seq + 1;

    y_start = w * ROWS_PER_WORKER;
    y_end = y_start + ROWS_PER_WORKER;
    if (y_end > SCREEN_HEIGHT)
      y_end = SCREEN_HEIGHT;
    write_u32(assign, 0, (unsigned int)w);
    write_u32(assign, 4, (unsigned int)y_start);
    write_u32(assign, 8, (unsigned int)y_end);

    fnp_send(get_worker_mac(w), FNP_TYPE_CLUSTER_ASSIGN, tx_seq, 0,
             assign, 12, frame_buf);
    tx_seq = tx_seq + 1;
  }
}

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

  global_chunk = worker_id * CHUNKS_PER_WORKER + (int)chunk_index;
  if (global_chunk < 0 || global_chunk >= TOTAL_CHUNKS)
    return 0;

  if (chunks_received[global_chunk])
    return 0;

  pixel_offset = worker_id * PIXELS_PER_WORKER + (int)chunk_index * CHUNK_MAX_PIXELS;

  i = 0;
  while (i < pixel_count && (pixel_offset + i) < (SCREEN_WIDTH * SCREEN_HEIGHT))
  {
    int px;
    px = data[CHUNK_HEADER_SIZE + i] & 0xFF;
    backbuf[pixel_offset + i] = px;
    /* VRAMPX is byte-addressable. */
    __builtin_storeb(PIXEL_FB_ADDR + (pixel_offset + i), px);
    i = i + 1;
  }

  chunks_received[global_chunk] = 1;
  total_chunks_done = total_chunks_done + 1;

  return 1;
}

void blit_backbuf(void)
{
  int i;
  for (i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++)
  {
    __builtin_storeb(PIXEL_FB_ADDR + i, backbuf[i]);
  }
}

void upscale_to_fb(void)
{
  int ox, oy, sx, sy;

  for (oy = 0; oy < SCREEN_HEIGHT; oy++)
  {
    sy = UPSCALE_Y0 + (oy * UPSCALE_H) / SCREEN_HEIGHT;
    for (ox = 0; ox < SCREEN_WIDTH; ox++)
    {
      sx = UPSCALE_X0 + (ox * UPSCALE_W) / SCREEN_WIDTH;
      __builtin_storeb(PIXEL_FB_ADDR + (oy * SCREEN_WIDTH + ox),
                       backbuf[sy * SCREEN_WIDTH + sx]);
    }
  }
}

void reset_collection(void)
{
  int i;
  for (i = 0; i < TOTAL_CHUNKS; i++)
    chunks_received[i] = 0;
  total_chunks_done = 0;
}

int try_collect_packets(void)
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
  int need_ack;
  int ack_seq;
  int ack_mac[6];

  collected = 0;
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

      if (msg_type == FNP_TYPE_CLUSTER_RESULT)
      {
        worker_id = identify_worker(src_mac);
        if (worker_id >= 0)
          collected = collected + process_result_chunk(worker_id, rx_data, rx_data_len);
      }

      if (need_ack)
      {
        ack_data[0] = (ack_seq >> 8) & 0xFF;
        ack_data[1] = ack_seq & 0xFF;
        fnp_send(ack_mac, FNP_TYPE_ACK, 0, 0, ack_data, 2, frame_buf);
      }
    }
  }
  return collected;
}

int collect_all(int timeout_ms)
{
  int elapsed;

  elapsed = 0;
  while (total_chunks_done < TOTAL_CHUNKS && elapsed < timeout_ms)
  {
    if (sys_net_packet_count() > 0)
      try_collect_packets();
    else
    {
      sys_delay(5);
      elapsed = elapsed + 5;
    }
  }
  return (total_chunks_done >= TOTAL_CHUNKS) ? 1 : 0;
}

void update_max_iter(void)
{
  int iter;

  if (scale.hi > 0)
  {
    max_iter_val = 60;
    return;
  }

  if      (scale.lo > 0x40000000) iter = 100;
  else if (scale.lo > 0x10000000) iter = 120;
  else if (scale.lo > 0x04000000) iter = 160;
  else if (scale.lo > 0x01000000) iter = 200;
  else if (scale.lo > 0x00400000) iter = 240;
  else if (scale.lo > 0x00100000) iter = 280;
  else if (scale.lo > 0x00040000) iter = 300;
  else if (scale.lo > 0x00010000) iter = 320;
  else if (scale.lo > 0x00004000) iter = 340;
  else if (scale.lo > 0x00001000) iter = 380;
  else                            iter = 410;

  max_iter_val = iter;
}

void reset_view(void)
{
  fp64_make(&center_re, -1, 0x41A1BEB0);
  fp64_make(&center_im, 0, 0x21C01C90);
  fp64_make(&scale, 3, 0x80000000);
  update_max_iter();
}

int main(void)
{
  int key;
  int keys;
  int running;
  struct fp64 zoom_factor;
  int ci;

  fnp_init();
  tx_seq = 0;
  current_palette = 3;

  g_pixpal_fd = sys_open("/dev/pixpal", O_WRONLY);
  if (g_pixpal_fd < 0)
  {
    sys_putstr("mbroth: cannot open /dev/pixpal\n");
    return 1;
  }
  g_tty_fd = sys_tty_open_raw(1);
  if (g_tty_fd < 0)
  {
    sys_putstr("mbroth: cannot open /dev/tty in raw mode\n");
    sys_close(g_pixpal_fd);
    return 1;
  }

  launch_workers();

  backbuf = (int *)sys_heap_alloc(SCREEN_WIDTH * SCREEN_HEIGHT);

  term_clear();
  for (ci = 0; ci < SCREEN_WIDTH * SCREEN_HEIGHT; ci++)
  {
    __builtin_storeb(PIXEL_FB_ADDR + ci, 0);
    backbuf[ci] = 0;
  }

  reset_view();
  auto_zoom = 1;

  load_palette(current_palette);
  apply_palette();

  fp64_make(&zoom_factor, 0, ZOOM_FACTOR_LO);

  // First frame: render without zoom preview
  dispatch_workers();
  reset_collection();
  collect_all(COLLECT_TIMEOUT_MS);
  blit_backbuf();

  // Main render loop
  running = 1;
  while (running)
  {
    while ((key = sys_tty_event_read(g_tty_fd, 0)) >= 0)
    {
      if (key == ' ')
        auto_zoom = !auto_zoom;
      else if (key == 'r' || key == 'R')
        reset_view();
      else if (key == 'p' || key == 'P')
      {
        current_palette = (current_palette + 1) % NUM_PALETTES;
        load_palette(current_palette);
        apply_palette();
      }
    }

    keys = sys_get_key_state();
    if (keys & KEYSTATE_ESCAPE)
    {
      running = 0;
      break;
    }

    if (!auto_zoom)
    {
      sys_delay(50);
      continue;
    }

    // Advance zoom: scale *= 0.90
    fp64_mul(&scale, &scale, &zoom_factor);
    update_max_iter();

    if (scale.hi == 0 && scale.lo < MIN_SCALE_LO)
    {
      sys_delay(3000);
      reset_view();
    }

    dispatch_workers();
    reset_collection();
    upscale_to_fb();
    collect_all(COLLECT_TIMEOUT_MS);
  }

  // Cleanup: restore default RRRGGGBB palette
  {
    int i;
    int defpal[256];
    for (i = 0; i < 256; i++)
    {
      int r3, g3, b2, r, g, b;
      r3 = (i >> 5) & 7;
      g3 = (i >> 2) & 7;
      b2 = i & 3;
      r = (r3 << 5) | (r3 << 2) | (r3 >> 1);
      g = (g3 << 5) | (g3 << 2) | (g3 >> 1);
      b = (b2 << 6) | (b2 << 4) | (b2 << 2) | b2;
      defpal[i] = (r << 16) | (g << 8) | b;
    }
    pixpal_load_all(defpal);
  }
  for (ci = 0; ci < SCREEN_WIDTH * SCREEN_HEIGHT; ci++)
    __builtin_storeb(PIXEL_FB_ADDR + ci, 0);
  term_clear();

  sys_close(g_tty_fd);
  sys_close(g_pixpal_fd);

  return 0;
}
