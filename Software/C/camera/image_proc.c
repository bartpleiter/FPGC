/*
 * image_proc.c — FPGC-Camera image processing pipeline
 *
 * Integer-only algorithms matching Tests/host/camera/image_proc.c.
 * Designed for the B32P3 CPU (no floats, no 64-bit ops needed).
 */
#include "image_proc.h"

void downsample_2x2(const unsigned char *in, unsigned char *out,
                    int in_w, int in_h)
{
    int out_w;
    int out_h;
    int oy;
    int ox;

    out_w = in_w / 2;
    out_h = in_h / 2;

    for (oy = 0; oy < out_h; oy++) {
        const unsigned char *row0;
        const unsigned char *row1;
        unsigned char *dst;
        int ix;
        int sum;

        row0 = in + (oy * 2) * in_w;
        row1 = row0 + in_w;
        dst = out + oy * out_w;

        for (ox = 0; ox < out_w; ox++) {
            ix = ox * 2;
            sum = row0[ix] + row0[ix + 1] + row1[ix] + row1[ix + 1];
            dst[ox] = (unsigned char)(sum / 4);
        }
    }
}

void auto_contrast(unsigned char *buf, int w, int h)
{
    int n;
    int i;
    int lo;
    int hi;
    int range;
    unsigned char lut[256];

    n = w * h;
    lo = 255;
    hi = 0;

    for (i = 0; i < n; i++) {
        if (buf[i] < lo) lo = buf[i];
        if (buf[i] > hi) hi = buf[i];
    }

    if (hi <= lo) return;

    range = hi - lo;

    /* Precompute 256-entry LUT (256 divisions instead of n) */
    for (i = 0; i < 256; i++) {
        if (i <= lo) {
            lut[i] = 0;
        } else if (i >= hi) {
            lut[i] = 255;
        } else {
            lut[i] = (unsigned char)(((i - lo) * 255) / range);
        }
    }

    /* Apply LUT — no division per pixel */
    for (i = 0; i < n; i++) {
        buf[i] = lut[buf[i]];
    }
}

/* Dashboy Camera dither threshold matrices (interleaved, 48 bytes) */
static unsigned char dither_patterns[48] = {
    0x2A, 0x5E, 0x9B, 0x51, 0x8B, 0xCA,
    0x33, 0x69, 0xA6, 0x5A, 0x97, 0xD6,
    0x44, 0x7C, 0xBA, 0x37, 0x6D, 0xAA,
    0x4D, 0x87, 0xC6, 0x40, 0x78, 0xB6,
    0x30, 0x65, 0xA2, 0x57, 0x93, 0xD2,
    0x2D, 0x61, 0x9E, 0x54, 0x8F, 0xCE,
    0x4A, 0x84, 0xC2, 0x3D, 0x74, 0xB2,
    0x47, 0x80, 0xBE, 0x3A, 0x71, 0xAE
};

static unsigned char mat_dg_b[16];
static unsigned char mat_lg_dg[16];
static unsigned char mat_w_lg[16];
static int dither_tables_ready = 0;

/* Externally accessible copies for combined dither+blit */
unsigned char mat_dg_b_ext[16];
unsigned char mat_lg_dg_ext[16];
unsigned char mat_w_lg_ext[16];

static void init_dither_tables(void)
{
    int pos;
    int idx;

    if (dither_tables_ready) return;
    idx = 0;
    for (pos = 0; pos < 16; pos++) {
        mat_dg_b[pos]  = dither_patterns[idx];
        mat_dg_b_ext[pos]  = dither_patterns[idx];
        idx++;
        mat_lg_dg[pos] = dither_patterns[idx];
        mat_lg_dg_ext[pos] = dither_patterns[idx];
        idx++;
        mat_w_lg[pos]  = dither_patterns[idx];
        mat_w_lg_ext[pos]  = dither_patterns[idx];
        idx++;
    }
    dither_tables_ready = 1;
}

void init_dither_tables_ext(void)
{
    init_dither_tables();
}

void dither_4x4(const unsigned char *in, unsigned char *out, int w, int h)
{
    int x;
    int y;
    int y4;
    int mi;
    unsigned char p;

    init_dither_tables();

    for (y = 0; y < h; y++) {
        const unsigned char *src_row;
        unsigned char *dst_row;

        src_row = in + y * w;
        dst_row = out + y * w;
        y4 = (y & 3) << 2;

        for (x = 0; x < w; x++) {
            p = src_row[x];
            mi = y4 + (x & 3);

            if (p < mat_dg_b[mi]) {
                dst_row[x] = 0;
            } else if (p < mat_lg_dg[mi]) {
                dst_row[x] = 1;
            } else if (p < mat_w_lg[mi]) {
                dst_row[x] = 2;
            } else {
                dst_row[x] = 3;
            }
        }
    }
}

/* Bayer 4×4 matrix for 8-shade ordered dithering */
static unsigned char bayer4[16] = {
     0,  8,  2, 10,
    12,  4, 14,  6,
     3, 11,  1,  9,
    15,  7, 13,  5
};

/* Externally accessible copy */
unsigned char bayer4_ext[16] = {
     0,  8,  2, 10,
    12,  4, 14,  6,
     3, 11,  1,  9,
    15,  7, 13,  5
};

void dither_8shade(const unsigned char *in, unsigned char *out, int w, int h)
{
    int x;
    int y;
    int y4;
    int mi;
    int v;

    for (y = 0; y < h; y++) {
        const unsigned char *src_row;
        unsigned char *dst_row;

        src_row = in + y * w;
        dst_row = out + y * w;
        y4 = (y & 3) << 2;

        for (x = 0; x < w; x++) {
            mi = y4 + (x & 3);
            v = ((int)src_row[x] + bayer4[mi] * 2 + 1) >> 5;
            if (v > 7) v = 7;
            dst_row[x] = (unsigned char)v;
        }
    }
}

void scale_2x(const unsigned char *in, unsigned char *out, int w, int h)
{
    int out_w;
    int y;
    int x;
    int ox;
    unsigned char v;

    out_w = w * 2;

    for (y = 0; y < h; y++) {
        const unsigned char *src_row;
        unsigned char *dst_row0;
        unsigned char *dst_row1;

        src_row = in + y * w;
        dst_row0 = out + (y * 2) * out_w;
        dst_row1 = dst_row0 + out_w;

        for (x = 0; x < w; x++) {
            v = src_row[x];
            ox = x * 2;
            dst_row0[ox]     = v;
            dst_row0[ox + 1] = v;
            dst_row1[ox]     = v;
            dst_row1[ox + 1] = v;
        }
    }
}
