/*
 * FPGC-Camera — Full Pipeline Test
 *
 * Runs the complete image processing pipeline on test images:
 *   1. Load 320×240 PGM input
 *   2. Downsample 2× → 160×120
 *   3. Auto-contrast (histogram stretch)
 *   4. 4×4 ordered dithering → 2-bit output
 *   5. Scale 2× → 320×240 display preview
 *
 * Also generates intermediate outputs for inspection:
 *   - *_ds.pgm      — after downsample
 *   - *_ac.pgm      — after auto-contrast
 *   - *_dith.pgm    — after dithering (mapped to 0/85/170/255)
 *   - *_display.pgm — 2× scaled final (what HDMI would show)
 *
 * Usage: ./pipeline_test <input.pgm> [output_prefix]
 *        ./pipeline_test --generate   (generate synthetic test images)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "image_proc.h"
#include "pgm_io.h"

/* Map 2-bit shade to 8-bit for PGM output */
static const u8 shade_to_grey[4] = { 0, 85, 170, 255 };

static void map_shades(const u8 *in, u8 *out, int n)
{
    int i;
    for (i = 0; i < n; i++) {
        out[i] = shade_to_grey[in[i]];
    }
}

/* Generate a 320×240 gradient test image (horizontal gradient) */
static void generate_gradient(u8 *buf, int w, int h)
{
    int y, x;
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            buf[y * w + x] = (u8)((x * 255) / (w - 1));
        }
    }
}

/* Generate a 320×240 image with zones of different brightness */
static void generate_zones(u8 *buf, int w, int h)
{
    int y, x;
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            /* 4 vertical strips at different brightness levels */
            int zone = (x * 4) / w;
            u8 base;
            switch (zone) {
                case 0: base = 20;  break;  /* near black */
                case 1: base = 80;  break;  /* dark */
                case 2: base = 170; break;  /* light */
                default: base = 240; break; /* near white */
            }
            /* Add some per-pixel variation based on position */
            int noise = ((x * 7 + y * 13) & 31) - 16;
            int v = base + noise;
            if (v < 0) v = 0;
            if (v > 255) v = 255;
            buf[y * w + x] = (u8)v;
        }
    }
}

/* Generate a 320×240 low-contrast image (simulates underexposed scene) */
static void generate_lowcontrast(u8 *buf, int w, int h)
{
    int y, x;
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            /* Checkerboard-ish pattern compressed to [80, 140] range */
            int checker = ((x / 20) + (y / 20)) & 1;
            int base = checker ? 130 : 90;
            int noise = ((x * 3 + y * 7) & 15) - 8;
            int v = base + noise;
            if (v < 0) v = 0;
            if (v > 255) v = 255;
            buf[y * w + x] = (u8)v;
        }
    }
}

static int process_image(const char *input_path, const char *prefix)
{
    int w, h;
    u8 *img;
    u8 ds_buf[160 * 120];
    u8 dith_buf[160 * 120];
    u8 mapped_buf[160 * 120];
    u8 display_buf[320 * 240];
    char path[256];

    /* Load input */
    img = pgm_read(input_path, &w, &h);
    if (!img) {
        fprintf(stderr, "Error: cannot read %s\n", input_path);
        return 1;
    }
    if (w != 320 || h != 240) {
        fprintf(stderr, "Error: expected 320x240, got %dx%d\n", w, h);
        free(img);
        return 1;
    }

    printf("Processing %s → %s_*\n", input_path, prefix);

    /* Step 1: Downsample */
    downsample_2x2(img, ds_buf, 320, 240);
    snprintf(path, sizeof(path), "%s_ds.pgm", prefix);
    pgm_write(path, ds_buf, 160, 120);
    printf("  downsample → %s (160x120)\n", path);

    /* Step 2: Auto-contrast */
    auto_contrast(ds_buf, 160, 120);
    snprintf(path, sizeof(path), "%s_ac.pgm", prefix);
    pgm_write(path, ds_buf, 160, 120);
    printf("  auto-contrast → %s\n", path);

    /* Step 3: Dither */
    dither_4x4(ds_buf, dith_buf, 160, 120);
    map_shades(dith_buf, mapped_buf, 160 * 120);
    snprintf(path, sizeof(path), "%s_dith.pgm", prefix);
    pgm_write(path, mapped_buf, 160, 120);
    printf("  dither → %s (4 shades)\n", path);

    /* Step 4: Scale 2× for display preview */
    scale_2x(dith_buf, display_buf, 160, 120);
    /* Map the scaled 2-bit values to greyscale */
    map_shades(display_buf, display_buf, 320 * 240);
    snprintf(path, sizeof(path), "%s_display.pgm", prefix);
    pgm_write(path, display_buf, 320, 240);
    printf("  display → %s (320x240, 2x scaled)\n", path);

    /* Print stats */
    {
        int shade_count[4] = {0, 0, 0, 0};
        int i;
        for (i = 0; i < 160 * 120; i++) {
            shade_count[dith_buf[i]]++;
        }
        printf("  shade distribution: B=%d DG=%d LG=%d W=%d\n",
               shade_count[0], shade_count[1],
               shade_count[2], shade_count[3]);
    }

    free(img);
    return 0;
}

static int generate_test_images(void)
{
    u8 buf[320 * 240];
    const char *dir = "testdata";

    printf("Generating synthetic test images in %s/\n", dir);

    generate_gradient(buf, 320, 240);
    {
        char path[256];
        snprintf(path, sizeof(path), "%s/gradient.pgm", dir);
        pgm_write(path, buf, 320, 240);
        printf("  %s — horizontal gradient\n", path);
    }

    generate_zones(buf, 320, 240);
    {
        char path[256];
        snprintf(path, sizeof(path), "%s/zones.pgm", dir);
        pgm_write(path, buf, 320, 240);
        printf("  %s — brightness zones with noise\n", path);
    }

    generate_lowcontrast(buf, 320, 240);
    {
        char path[256];
        snprintf(path, sizeof(path), "%s/lowcontrast.pgm", dir);
        pgm_write(path, buf, 320, 240);
        printf("  %s — low-contrast checker (tests auto-contrast)\n", path);
    }

    return 0;
}

int main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "--generate") == 0) {
        return generate_test_images();
    }

    if (argc < 2 || argc > 3) {
        fprintf(stderr, "Usage: %s <input.pgm> [output_prefix]\n", argv[0]);
        fprintf(stderr, "       %s --generate\n", argv[0]);
        return 1;
    }

    {
        const char *input = argv[1];
        const char *prefix;
        char auto_prefix[256];

        if (argc == 3) {
            prefix = argv[2];
        } else {
            /* Derive prefix from input filename */
            const char *base = strrchr(input, '/');
            base = base ? base + 1 : input;
            snprintf(auto_prefix, sizeof(auto_prefix), "output/%.*s",
                     (int)(strrchr(base, '.') ?
                           strrchr(base, '.') - base : (int)strlen(base)),
                     base);
            prefix = auto_prefix;
        }

        return process_image(input, prefix);
    }
}
