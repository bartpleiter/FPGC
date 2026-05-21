/*
 * FPGC-Camera — Image Processing Implementation
 *
 * All algorithms use only integer arithmetic to match B32P3 capabilities.
 */

#include "image_proc.h"

/* --- Downsample --------------------------------------------------------- */

void downsample_2x2(const u8 *in, u8 *out, int in_w, int in_h)
{
    int out_w = in_w / 2;
    int out_h = in_h / 2;
    int oy, ox;

    for (oy = 0; oy < out_h; oy++) {
        const u8 *row0 = in + (oy * 2) * in_w;
        const u8 *row1 = row0 + in_w;
        u8 *dst = out + oy * out_w;

        for (ox = 0; ox < out_w; ox++) {
            int ix = ox * 2;
            /* 2×2 box average (integer, rounds down) */
            int sum = row0[ix] + row0[ix + 1] + row1[ix] + row1[ix + 1];
            dst[ox] = (u8)(sum / 4);
        }
    }
}

/* --- Auto-contrast ------------------------------------------------------ */

void auto_contrast(u8 *buf, int w, int h)
{
    int n = w * h;
    int i;
    u8 lo = 255, hi = 0;

    /* Pass 1: find min and max */
    for (i = 0; i < n; i++) {
        if (buf[i] < lo) lo = buf[i];
        if (buf[i] > hi) hi = buf[i];
    }

    /* If range is 0 or already full, nothing to do */
    if (hi <= lo) return;

    /* Pass 2: linear remap [lo, hi] → [0, 255] using integer math.
     * out = (in - lo) * 255 / (hi - lo)
     */
    {
        int range = hi - lo;
        for (i = 0; i < n; i++) {
            int v = buf[i];
            if (v <= lo) {
                buf[i] = 0;
            } else if (v >= hi) {
                buf[i] = 255;
            } else {
                buf[i] = (u8)(((v - lo) * 255) / range);
            }
        }
    }
}

/* --- 4×4 Ordered Dithering ---------------------------------------------- */

/*
 * Threshold matrices from the Dashboy Camera project.
 * 48 bytes total, interleaved as: [DG_B_0, LG_DG_0, W_LG_0, DG_B_1, ...]
 * We de-interleave into three 4×4 matrices at compile time.
 *
 * Source: Dithering_patterns_regular from config.h
 * Layout: for each of 16 positions (row-major 4×4), three thresholds
 * are interleaved: threshold_black_to_darkgrey, threshold_dg_to_lg,
 * threshold_lg_to_white.
 *
 * Algorithm: for pixel value p at position (x,y):
 *   if p < mat_DG_B[y%4][x%4]  → shade 0 (black)
 *   elif p < mat_LG_DG[y%4][x%4] → shade 1 (dark grey)
 *   elif p < mat_W_LG[y%4][x%4]  → shade 2 (light grey)
 *   else → shade 3 (white)
 */

/* Interleaved source data */
static const u8 dither_patterns[48] = {
    0x2A, 0x5E, 0x9B, 0x51, 0x8B, 0xCA,
    0x33, 0x69, 0xA6, 0x5A, 0x97, 0xD6,
    0x44, 0x7C, 0xBA, 0x37, 0x6D, 0xAA,
    0x4D, 0x87, 0xC6, 0x40, 0x78, 0xB6,
    0x30, 0x65, 0xA2, 0x57, 0x93, 0xD2,
    0x2D, 0x61, 0x9E, 0x54, 0x8F, 0xCE,
    0x4A, 0x84, 0xC2, 0x3D, 0x74, 0xB2,
    0x47, 0x80, 0xBE, 0x3A, 0x71, 0xAE
};

/* De-interleaved threshold matrices (4×4 each, row-major) */
static u8 mat_dg_b[16];   /* black ↔ dark grey boundary */
static u8 mat_lg_dg[16];  /* dark grey ↔ light grey boundary */
static u8 mat_w_lg[16];   /* light grey ↔ white boundary */
static int dither_tables_ready = 0;

static void init_dither_tables(void)
{
    int pos, idx;
    if (dither_tables_ready) return;
    idx = 0;
    for (pos = 0; pos < 16; pos++) {
        mat_dg_b[pos]  = dither_patterns[idx++];
        mat_lg_dg[pos] = dither_patterns[idx++];
        mat_w_lg[pos]  = dither_patterns[idx++];
    }
    dither_tables_ready = 1;
}

void dither_4x4(const u8 *in, u8 *out, int w, int h)
{
    int x, y;
    init_dither_tables();

    for (y = 0; y < h; y++) {
        const u8 *src_row = in + y * w;
        u8 *dst_row = out + y * w;
        int y4 = (y & 3) << 2;  /* (y % 4) * 4 for matrix row offset */

        for (x = 0; x < w; x++) {
            u8 p = src_row[x];
            int mi = y4 + (x & 3);  /* matrix index */

            if (p < mat_dg_b[mi]) {
                dst_row[x] = 0;       /* black */
            } else if (p < mat_lg_dg[mi]) {
                dst_row[x] = 1;       /* dark grey */
            } else if (p < mat_w_lg[mi]) {
                dst_row[x] = 2;       /* light grey */
            } else {
                dst_row[x] = 3;       /* white */
            }
        }
    }
}

/* --- 2× Scale ----------------------------------------------------------- */

void scale_2x(const u8 *in, u8 *out, int w, int h)
{
    int out_w = w * 2;
    int y, x;

    for (y = 0; y < h; y++) {
        const u8 *src_row = in + y * w;
        u8 *dst_row0 = out + (y * 2) * out_w;
        u8 *dst_row1 = dst_row0 + out_w;

        for (x = 0; x < w; x++) {
            u8 v = src_row[x];
            int ox = x * 2;
            dst_row0[ox]     = v;
            dst_row0[ox + 1] = v;
            dst_row1[ox]     = v;
            dst_row1[ox + 1] = v;
        }
    }
}
