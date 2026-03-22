//
// mbrotc.c — Cluster Mandelbrot worker program (userBDOS).
//
// Receives view parameters and row assignments from the coordinator via FNP,
// computes Mandelbrot pixels using the FP64 coprocessor, and sends results
// back in chunks.
//

#include <syscall.h>
#include <fnp.h>

// ---- Screen constants ----
#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240

// ---- Chunk constants ----
#define CHUNK_HEADER_SIZE  4
#define CHUNK_MAX_PIXELS   1020
#define CHUNK_MAX_PAYLOAD  (CHUNK_HEADER_SIZE + CHUNK_MAX_PIXELS)

// ---- Maximum iterations ----
#define DEFAULT_MAX_ITER 128

// ---- View parameters (received from coordinator) ----
int center_re_hi;
unsigned int center_re_lo;
int center_im_hi;
unsigned int center_im_lo;
int scale_hi;
unsigned int scale_lo;
int max_iter;

// ---- Assignment (received from coordinator) ----
int assign_y_start;
int assign_y_end;

// ---- Coordinator MAC ----
int coord_mac[6];

// ---- Buffers ----
char frame_buf[FNP_FRAME_BUF_SIZE];
char chunk_payload[CHUNK_MAX_PAYLOAD];
char pixel_buf[SCREEN_WIDTH];

// ---- Sequence counter ----
int tx_seq;

// ---- State ----
int has_params;
int has_assign;
int our_worker_id;

// ---- Assembly FP64 helpers (mbrotc_asm.asm) ----
extern void mbrotc_load_cre(int hi, int lo);
extern void mbrotc_load_cim(int hi, int lo);
extern void mbrotc_load_step(int hi, int lo);
extern void mbrotc_advance_cre(void);
extern void mbrotc_advance_cim(void);
extern int  mbrotc_store_hi_cre(void);
extern int  mbrotc_store_lo_cre(void);
extern int  mbrotc_store_hi_step(void);
extern int  mbrotc_store_lo_step(void);
extern void mbrotc_load_f6(int hi, int lo);
extern void mbrotc_load_f7(int hi, int lo);
extern void mbrotc_mul_f7_f6(void);
extern void mbrotc_sub_f0_f6(void);
extern void mbrotc_sub_f1_f6(void);
extern int  mbrotc_store_hi_f7(void);
extern int  mbrotc_store_lo_f7(void);
extern int  mbrotc_mandelbrot_pixel(int max_iter);

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

// ---- Parse CLUSTER_PARAMS ----
void parse_params(char *data, int data_len)
{
  if (data_len < 28)
    return;

  center_re_hi = (int)read_u32(data, 0);
  center_re_lo = read_u32(data, 4);
  center_im_hi = (int)read_u32(data, 8);
  center_im_lo = read_u32(data, 12);
  scale_hi     = (int)read_u32(data, 16);
  scale_lo     = read_u32(data, 20);
  max_iter     = (int)read_u32(data, 24);

  has_params = 1;
}

// ---- Parse CLUSTER_ASSIGN ----
void parse_assign(char *data, int data_len)
{
  int target_id;

  if (data_len < 12)
    return;

  target_id = (int)read_u32(data, 0);
  if (target_id != our_worker_id)
    return;

  assign_y_start = (int)read_u32(data, 4);
  assign_y_end   = (int)read_u32(data, 8);

  has_assign = 1;
}

// ---- Send one CLUSTER_RESULT chunk ----
void send_chunk(int chunk_index, char *pixels, int pixel_count)
{
  int payload_len;
  int i;

  write_u32(chunk_payload, 0, (unsigned int)chunk_index);
  i = 0;
  while (i < pixel_count)
  {
    chunk_payload[CHUNK_HEADER_SIZE + i] = pixels[i];
    i = i + 1;
  }

  payload_len = CHUNK_HEADER_SIZE + pixel_count;

  fnp_send_reliable(coord_mac, FNP_TYPE_CLUSTER_RESULT,
                    chunk_payload, payload_len, frame_buf, &tx_seq);
}

// ---- Compute assigned rows and send results ----
void compute_and_send(void)
{
  int step_hi_cpu;
  unsigned int step_lo_cpu;
  int start_re_hi;
  unsigned int start_re_lo;
  int y;
  int x;
  int chunk_index;
  int chunk_pixel_count;
  char chunk_pixels[CHUNK_MAX_PIXELS];

  // Compute pixel step = scale / 320
  // 1/320 in Q32.32: {0, 0x00CCCCCD}
  mbrotc_load_f6(0, 0x00CCCCCD);
  mbrotc_load_f7(scale_hi, scale_lo);
  mbrotc_mul_f7_f6();  // f7 = scale * (1/320)

  step_hi_cpu = mbrotc_store_hi_f7();
  step_lo_cpu = mbrotc_store_lo_f7();

  // Load step into f7 for advance operations
  mbrotc_load_step(step_hi_cpu, step_lo_cpu);

  // Compute start_re = center_re - scale * 0.5
  mbrotc_load_f6(0, 0x80000000);  // 0.5
  mbrotc_load_f7(scale_hi, scale_lo);
  mbrotc_mul_f7_f6();  // f7 = scale * 0.5 = half_width
  // Move half_width to f6 for subtraction
  {
    int hw_hi = mbrotc_store_hi_f7();
    unsigned int hw_lo = mbrotc_store_lo_f7();
    mbrotc_load_f6(hw_hi, hw_lo);
  }
  mbrotc_load_cre(center_re_hi, center_re_lo);  // f0 = center_re
  mbrotc_sub_f0_f6();  // f0 = center_re - half_width = start_re

  start_re_hi = mbrotc_store_hi_cre();
  start_re_lo = mbrotc_store_lo_cre();

  // Compute start_im for our first row
  // start_im = center_im - step * (120 - y_start)
  mbrotc_load_f6(120 - assign_y_start, 0);
  mbrotc_load_f7(step_hi_cpu, step_lo_cpu);
  mbrotc_mul_f7_f6();  // f7 = step * (120 - y_start)
  {
    int off_hi = mbrotc_store_hi_f7();
    unsigned int off_lo = mbrotc_store_lo_f7();
    mbrotc_load_f6(off_hi, off_lo);
  }
  mbrotc_load_cim(center_im_hi, center_im_lo);  // f1 = center_im
  mbrotc_sub_f1_f6();  // f1 = center_im - offset = start_im

  // Initialize chunk tracking
  chunk_index = 0;
  chunk_pixel_count = 0;

  for (y = assign_y_start; y < assign_y_end; y++)
  {
    // Reset c_re to start_re for this row
    mbrotc_load_cre(start_re_hi, start_re_lo);

    for (x = 0; x < SCREEN_WIDTH; x++)
    {
      int iter = mbrotc_mandelbrot_pixel(max_iter);

      if (iter == 0)
        pixel_buf[x] = 0;
      else
        pixel_buf[x] = ((iter - 1) % 255) + 1;

      // Advance c_re by step
      mbrotc_load_step(step_hi_cpu, step_lo_cpu);
      mbrotc_advance_cre();
    }

    // Copy row pixels into chunk buffer
    x = 0;
    while (x < SCREEN_WIDTH)
    {
      chunk_pixels[chunk_pixel_count] = pixel_buf[x];
      chunk_pixel_count = chunk_pixel_count + 1;
      x = x + 1;

      if (chunk_pixel_count >= CHUNK_MAX_PIXELS)
      {
        send_chunk(chunk_index, chunk_pixels, chunk_pixel_count);
        chunk_index = chunk_index + 1;
        chunk_pixel_count = 0;
      }
    }

    // Advance c_im by step
    mbrotc_load_step(step_hi_cpu, step_lo_cpu);
    mbrotc_advance_cim();
  }

  if (chunk_pixel_count > 0)
  {
    send_chunk(chunk_index, chunk_pixels, chunk_pixel_count);
  }
}

// ---- Print decimal integer ----
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

int main(void)
{
  int our_mac[6];
  int rxlen;
  int src_mac[6];
  int msg_type;
  int rx_seq;
  int rx_flags;
  char *rx_data;
  int rx_data_len;
  int frame_count;

  fnp_init();
  fnp_get_our_mac(our_mac);

  our_worker_id = (our_mac[5] & 0xFF) - 0x02;

  tx_seq = 0;
  has_params = 0;
  has_assign = 0;
  max_iter = DEFAULT_MAX_ITER;
  frame_count = 0;

  sys_print_str("Mandelbrot Worker ");
  {
    char id_char;
    id_char = '0' + our_worker_id;
    sys_print_char(id_char);
  }
  sys_print_str(" ready\n");
  sys_print_str("Waiting for assignments...\n");

  while (1)
  {
    if (sys_net_packet_count() > 0)
    {
      rxlen = sys_net_recv(frame_buf, FNP_FRAME_BUF_SIZE);

      if (rxlen >= 14)
      {
        int dest_ok;
        dest_ok = 1;
        {
          int dm;
          for (dm = 0; dm < 6; dm++)
          {
            if ((frame_buf[dm] & 0xFF) != (our_mac[dm] & 0xFF))
            {
              dest_ok = 0;
              break;
            }
          }
        }

        if (dest_ok && fnp_parse(frame_buf, rxlen, src_mac,
                                  &msg_type, &rx_seq, &rx_flags,
                                  &rx_data, &rx_data_len))
        {
          {
            int i;
            i = 0;
            while (i < 6)
            {
              coord_mac[i] = src_mac[i];
              i = i + 1;
            }
          }

          if (msg_type == FNP_TYPE_CLUSTER_PARAMS)
          {
            parse_params(rx_data, rx_data_len);
          }
          else if (msg_type == FNP_TYPE_CLUSTER_ASSIGN)
          {
            parse_assign(rx_data, rx_data_len);

            if (has_params && has_assign)
            {
              frame_count = frame_count + 1;
              sys_print_str("Frame ");
              print_int(frame_count);
              sys_print_str(": rows ");
              print_int(assign_y_start);
              sys_print_str("-");
              print_int(assign_y_end);
              sys_print_str("...");

              compute_and_send();

              sys_print_str(" done\n");

              has_assign = 0;
            }
          }
        }
      }
    }

    sys_delay(1);
  }

  return 0;
}
