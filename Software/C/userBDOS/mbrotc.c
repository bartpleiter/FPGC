//
// mbrotc.c — Cluster Mandelbrot worker program (userBDOS).
//
// Receives view parameters and row assignments from the coordinator via FNP,
// computes Mandelbrot pixels using the FP64 coprocessor, and sends results
// back in chunks.
//
// Protocol:
//   1. Coordinator sends CLUSTER_PARAMS (center, scale, max_iter)
//   2. Coordinator sends CLUSTER_ASSIGN (y_start, y_end)
//   3. Worker computes rows, sends CLUSTER_RESULT chunks as they fill
//   4. Repeat from step 1 for next frame
//

#define USER_SYSCALL
#define USER_FNP
#include "libs/user/user.h"

// ---- Screen constants ----
#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240

// ---- Chunk constants ----
// Each CLUSTER_RESULT: 4 bytes chunk_index + up to 1020 bytes pixel data
#define CHUNK_HEADER_SIZE  4
#define CHUNK_MAX_PIXELS   1020
#define CHUNK_MAX_PAYLOAD  (CHUNK_HEADER_SIZE + CHUNK_MAX_PIXELS)

// ---- FP64 register allocation ----
#define F_CRE  0  // c_re (per-pixel)
#define F_CIM  1  // c_im (per-row)
#define F_ZRE  2  // z_re (iteration)
#define F_ZIM  3  // z_im (iteration)
#define F_TMP1 4  // scratch
#define F_TMP2 5  // scratch
#define F_TMP3 6  // scratch
#define F_STEP 7  // pixel step

// ---- Maximum iterations (overridden by coordinator) ----
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

// ---- Coordinator MAC (saved from received packets) ----
int coord_mac[6];

// ---- Buffers ----
char frame_buf[FNP_FRAME_BUF_SIZE];
char chunk_payload[CHUNK_MAX_PAYLOAD];
char pixel_buf[SCREEN_WIDTH]; // one row of pixels

// ---- Sequence counter ----
int tx_seq;

// ---- State ----
int has_params;
int has_assign;
int our_worker_id;  // Our worker index (0-3), derived from MAC last byte

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

// ---- Optimized Mandelbrot iteration for one pixel ----
// Assumes F_CRE (f0) and F_CIM (f1) are loaded with the complex coordinate.
// Returns iteration count (0 = in set, 1..max_iter = escaped).
//
// OPTIMIZATION: Entire iteration loop is inline assembly to eliminate B32CC's
// per-iteration stack traffic (~9 memory ops/iteration in the C version).
// CPU registers r4=max_iter, r5=iter are kept in registers throughout.
// FP registers (separate file) are used for computation with no conflicts.
// Do-while loop structure saves one branch per iteration.
int mandelbrot_pixel()
{
  int retval = 0;
  asm(
    // CPU registers: r4=max_iter, r5=iter, r1=temp/return, r11=momentary
    // FP regs (separate file): f0=c_re, f1=c_im, f2=z_re, f3=z_im,
    //   f4=z_re^2/|z|^2, f5=z_im^2, f6=z_re*z_im

    // Load max_iter from global into CPU r4
    "addr2reg Label_max_iter r11"
    "read 0 r11 r4"

    // z_re = z_im = 0
    "fld r0 r0 r2"
    "fld r0 r0 r3"

    // iter = 0
    "or r0 r0 r5"

    // Edge case: max_iter == 0
    "beq r4 r0 Label_mbrot_asm_set"

    "Label_mbrot_asm_loop:"
    // --- Mandelbrot iteration (all FP register file, no CPU conflict) ---
    "fmul r2 r2 r4       ; f4 = z_re^2"
    "fmul r3 r3 r5       ; f5 = z_im^2"
    "fmul r2 r3 r6       ; f6 = z_re * z_im"
    "fsub r4 r5 r2       ; f2 = z_re^2 - z_im^2"
    "fadd r2 r0 r2       ; f2 += c_re  (new z_re)"
    "fadd r6 r6 r3       ; f3 = 2 * z_re * z_im"
    "fadd r3 r1 r3       ; f3 += c_im  (new z_im)"

    // Escape check: |z|^2 >= 4 (unsigned compare handles overflow)
    "fadd r4 r5 r4       ; f4 = z_re^2 + z_im^2 = |z|^2"
    "fsthi r4 r0 r1      ; r1 = integer part of |z|^2"
    "sltu r1 4 r1        ; r1 = 1 if mag < 4 (not escaped)"
    "beq r1 r0 Label_mbrot_asm_escaped"

    // Continue: iter++, check < max_iter
    "add r5 1 r5"
    "slt r5 r4 r1        ; iter < max_iter? (CPU r5 vs CPU r4)"
    "bne r1 r0 Label_mbrot_asm_loop"

    // Fall through: reached max_iter (in set)
    "Label_mbrot_asm_set:"
    "write -4 r14 r0     ; retval = 0"
    "jump Label_mbrot_asm_done"

    "Label_mbrot_asm_escaped:"
    "add r5 1 r1         ; r1 = iter + 1"
    "write -4 r14 r1     ; retval = iter + 1"

    "Label_mbrot_asm_done:"
  );
  return retval;
}

// ---- Parse CLUSTER_PARAMS from received data ----
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

// ---- Parse CLUSTER_ASSIGN from received data ----
// New format: 12 bytes = worker_id(4) + y_start(4) + y_end(4)
// Only accepts if worker_id matches our own.
void parse_assign(char *data, int data_len)
{
  int target_id;

  if (data_len < 12)
    return;

  target_id = (int)read_u32(data, 0);
  if (target_id != our_worker_id)
    return;  // Not for us

  assign_y_start = (int)read_u32(data, 4);
  assign_y_end   = (int)read_u32(data, 8);

  has_assign = 1;
}

// ---- Send one CLUSTER_RESULT chunk (reliably, with ACK) ----
void send_chunk(int chunk_index, char *pixels, int pixel_count)
{
  int payload_len;
  int i;

  // Build payload: 4-byte chunk_index + pixel data
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
// OPTIMIZATIONS: inline asm mandelbrot_pixel, reliable chunk sending.
// Original chunk structure preserved for debugging.
void compute_and_send()
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
  __fld(6, 0, 0x00CCCCCD);
  __fld(F_STEP, scale_hi, scale_lo);
  __fmul(F_STEP, F_STEP, 6);

  step_hi_cpu = __fsthi(F_STEP);
  step_lo_cpu = __fstlo(F_STEP);

  // Compute start_re = center_re - scale * 0.5
  __fld(F_TMP2, 0, 0x80000000);     // 0.5
  __fld(6, scale_hi, scale_lo);
  __fmul(6, 6, F_TMP2);              // half_width
  __fld(F_CRE, center_re_hi, center_re_lo);
  __fsub(F_CRE, F_CRE, 6);          // start_re

  start_re_hi = __fsthi(F_CRE);
  start_re_lo = __fstlo(F_CRE);

  // Compute start_im for our first row
  // start_im = center_im - step * (120 - y_start)
  __fld(6, 120 - assign_y_start, 0);
  __fld(F_STEP, step_hi_cpu, step_lo_cpu);
  __fmul(6, F_STEP, 6);              // step * (120 - y_start)
  __fld(F_CIM, center_im_hi, center_im_lo);
  __fsub(F_CIM, F_CIM, 6);           // c_im for first assigned row

  // Initialize chunk tracking
  chunk_index = 0;
  chunk_pixel_count = 0;

  for (y = assign_y_start; y < assign_y_end; y++)
  {
    // Reset c_re to start_re for this row
    __fld(F_CRE, start_re_hi, start_re_lo);

    for (x = 0; x < SCREEN_WIDTH; x++)
    {
      int iter = mandelbrot_pixel();

      // Store iteration as palette index
      // Index 0 = in set (black), 1-255 = escaped
      if (iter == 0)
      {
        pixel_buf[x] = 0;
      }
      else
      {
        pixel_buf[x] = ((iter - 1) % 255) + 1;
      }

      // Advance c_re by step
      __fld(F_STEP, step_hi_cpu, step_lo_cpu);
      __fadd(F_CRE, F_CRE, F_STEP);
    }

    // Copy row pixels into chunk buffer
    x = 0;
    while (x < SCREEN_WIDTH)
    {
      chunk_pixels[chunk_pixel_count] = pixel_buf[x];
      chunk_pixel_count = chunk_pixel_count + 1;
      x = x + 1;

      // If chunk is full, send it
      if (chunk_pixel_count >= CHUNK_MAX_PIXELS)
      {
        send_chunk(chunk_index, chunk_pixels, chunk_pixel_count);
        chunk_index = chunk_index + 1;
        chunk_pixel_count = 0;
      }
    }

    // Advance c_im by step
    __fld(F_STEP, step_hi_cpu, step_lo_cpu);
    __fadd(F_CIM, F_CIM, F_STEP);
  }

  // Send any remaining pixels in the last partial chunk
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

int main()
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

  // Initialize
  fnp_init();
  fnp_get_our_mac(our_mac);

  // Derive worker_id from our MAC last byte (MAC :02 = worker 0, :03 = 1, etc.)
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

  // Main loop: receive commands and compute
  while (1)
  {
    // Poll for incoming packets
    if (sys_net_packet_count() > 0)
    {
      rxlen = sys_net_recv(frame_buf, FNP_FRAME_BUF_SIZE);

      // Check destination MAC: only process frames addressed to us.
      // Destination MAC is in frame_buf[0..5].
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
          // Save coordinator MAC from any received frame
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

            // If we have both params and assignment, compute
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

              has_assign = 0; // ready for next assignment
            }
          }
        }
      }
    }

    // Small delay to avoid busy-spinning too hard
    sys_delay(1);
  }

  return 0;
}
