/*
 * viewfinder.c - Live viewfinder display via CAM2VRAM DMA
 *
 * Handles palette setup, dither table loading, auto-contrast LUT
 * computation from hardware min/max stats, and the main viewfinder
 * loop that runs CAM2VRAM with inline LUT + dithering.
 *
 * QVGA (320×240): CAM2VRAM direct — zero CPU cost per frame.
 * QQVGA (160×120): CAM2VRAM with 2× hardware upscale — each source
 *   byte is written to 4 VRAMPX positions (2×2 pixel doubling).
 */
#include "viewfinder.h"
#include "image_proc.h"
#include "uart.h"
#include "dma.h"
#include "fpgc.h"
#include "sys.h"
#include "cam_driver.h"
#include "ov7670_init.h"

/* Sensor dimensions */
#define SENS_W  320
#define SENS_H  240
#define SENS_BYTES (SENS_W * SENS_H)

/* 4-shade Game Boy palette: black, dark grey, light grey, white */
void setup_palette_4shade(void)
{
    unsigned int *pal;
    pal = (unsigned int *)FPGC_GPU_PIXEL_PALETTE;
    pal[0] = 0x000000;
    pal[1] = 0x555555;
    pal[2] = 0xAAAAAA;
    pal[3] = 0xFFFFFF;
}

/* 8-shade evenly spaced greyscale palette */
void setup_palette_8shade(void)
{
    unsigned int *pal;
    int i;
    unsigned int grey;
    pal = (unsigned int *)FPGC_GPU_PIXEL_PALETTE;
    for (i = 0; i < 8; i++) {
        grey = (unsigned int)(i * 255 / 7);
        pal[i] = (grey << 16) | (grey << 8) | grey;
    }
}

/* Full 256-shade greyscale ramp */
void setup_palette_greyscale(void)
{
    unsigned int *pal;
    int i;
    unsigned int grey;
    pal = (unsigned int *)FPGC_GPU_PIXEL_PALETTE;
    for (i = 0; i < 256; i++) {
        grey = (unsigned int)i;
        pal[i] = (grey << 16) | (grey << 8) | grey;
    }
}

/* ---- MEM2VRAM blit helpers (for capture preview or fallback) ---- */

void blit_raw_dma(unsigned int src_addr)
{
    cache_flush_data();
    dma_start_mem2vram_ex(
        (unsigned int)FPGC_GPU_PIXEL_DATA, src_addr,
        (unsigned int)SENS_BYTES, 0);
    while (dma_busy()) { }
}

void blit_lut_dma(unsigned int src_addr)
{
    cache_flush_data();
    dma_start_mem2vram_ex(
        (unsigned int)FPGC_GPU_PIXEL_DATA, src_addr,
        (unsigned int)SENS_BYTES,
        FPGC_DMA_CTRL_LUT_EN);
    while (dma_busy()) { }
}

void blit_dithered_dma(unsigned int src_addr, int use_lut)
{
    unsigned int flags;
    flags = FPGC_DMA_CTRL_DITHER_EN;
    if (use_lut) flags = flags | FPGC_DMA_CTRL_LUT_EN;
    cache_flush_data();
    dma_start_mem2vram_ex(
        (unsigned int)FPGC_GPU_PIXEL_DATA, src_addr,
        (unsigned int)SENS_BYTES, flags);
    while (dma_busy()) { }
}

void blit_dithered8_dma(unsigned int src_addr, int use_lut)
{
    unsigned int flags;
    flags = FPGC_DMA_CTRL_DITHER_EN | FPGC_DMA_CTRL_DITHER_8;
    if (use_lut) flags = flags | FPGC_DMA_CTRL_LUT_EN;
    cache_flush_data();
    dma_start_mem2vram_ex(
        (unsigned int)FPGC_GPU_PIXEL_DATA, src_addr,
        (unsigned int)SENS_BYTES, flags);
    while (dma_busy()) { }
}

/* ---- Auto-contrast from hardware min/max ---- */

static unsigned char ac_lut[256];
static int ac_lut_valid = 0;
static int ac_lut_counter = 0;

void auto_contrast_from_hw(void)
{
    unsigned int stats;
    int lo;
    int hi;
    int range;
    int i;

    stats = dma_drain_stats();
    lo = (int)DMA_DRAIN_MIN(stats);
    hi = (int)DMA_DRAIN_MAX(stats);

    if (hi <= lo) return;

    range = hi - lo;

    for (i = 0; i < 256; i++) {
        if (i <= lo) ac_lut[i] = 0;
        else if (i >= hi) ac_lut[i] = 255;
        else ac_lut[i] = (unsigned char)(((i - lo) * 255) / range);
    }

    /* Upload LUT to DMA engine */
    for (i = 0; i < 256; i++) {
        dma_lut_write(i, (int)ac_lut[i]);
    }
    ac_lut_valid = 1;
}

void auto_contrast_reset(void)
{
    ac_lut_valid = 0;
    ac_lut_counter = 0;
}

/* ---- Dither table loading ---- */

void load_dither_tables(void)
{
    int mi;

    init_dither_tables_ext();

    for (mi = 0; mi < 16; mi++) {
        dma_dither_thresh_write(0, mi, (int)mat_dg_b_ext[mi]);
        dma_dither_thresh_write(1, mi, (int)mat_lg_dg_ext[mi]);
        dma_dither_thresh_write(2, mi, (int)mat_w_lg_ext[mi]);
    }
    for (mi = 0; mi < 16; mi++) {
        dma_dither_bayer_write(mi, (int)bayer4_ext[mi]);
    }
}

/* ---- Viewfinder loop (forward declarations for keyboard) ---- */

/* These are provided by main.c */
extern int display_mode;
extern void set_mode(int mode);
extern int keyboard_poll(void);
extern void keyboard_check_connect(void);

/* Current resolution mode */
static int res_mode = RES_QVGA;

#define AC_INTERVAL 8

/* ---- QVGA viewfinder loop (CAM2VRAM direct) ---- */
static void viewfinder_qvga(void)
{
    unsigned int fps_start;
    int fps_frames;
    unsigned int cam2vram_flags;

    fps_start = get_micros();
    fps_frames = 0;

    /* First frame: VSYNC-synced CAM2VRAM (no LUT yet) */
    cam2vram_flags = 0;
    if (display_mode == MODE_DITH)
        cam2vram_flags = FPGC_DMA_CTRL_DITHER_EN;
    else if (display_mode == MODE_DITH8)
        cam2vram_flags = FPGC_DMA_CTRL_DITHER_EN | FPGC_DMA_CTRL_DITHER_8;

    dma_start_cam2vram((unsigned int)FPGC_GPU_PIXEL_DATA,
                       CAM_FRAME_BYTES, cam2vram_flags);
    while (dma_busy()) { }

    if (display_mode != MODE_RAW) {
        auto_contrast_from_hw();
    }

    while (1) {
        unsigned int t_start;
        unsigned int t_end;
        int key;

        t_start = get_micros();

        cam2vram_flags = 0;
        if (display_mode == MODE_DITH)
            cam2vram_flags = FPGC_DMA_CTRL_DITHER_EN;
        else if (display_mode == MODE_DITH8)
            cam2vram_flags = FPGC_DMA_CTRL_DITHER_EN | FPGC_DMA_CTRL_DITHER_8;
        if (ac_lut_valid && display_mode != MODE_RAW)
            cam2vram_flags = cam2vram_flags | FPGC_DMA_CTRL_LUT_EN;

        dma_start_cam2vram_immediate((unsigned int)FPGC_GPU_PIXEL_DATA,
                                     CAM_FRAME_BYTES, cam2vram_flags);

        while (dma_busy()) {
            key = keyboard_poll();
            if (key == 'r' || key == 'R') {
                set_mode(MODE_RAW);
                auto_contrast_reset();
            } else if (key == 'd' || key == 'D') {
                set_mode(MODE_DITH);
                auto_contrast_reset();
            } else if (key == 'e' || key == 'E') {
                set_mode(MODE_DITH8);
                auto_contrast_reset();
            } else if (key == 'q' || key == 'Q') {
                /* Switch to QQVGA — need to reconfigure sensor */
                while (dma_busy()) { }
                res_mode = RES_QQVGA;
                return;  /* exit to viewfinder_run for mode switch */
            }
        }

        if (display_mode != MODE_RAW) {
            ac_lut_counter = ac_lut_counter - 1;
            if (ac_lut_counter <= 0) {
                auto_contrast_from_hw();
                ac_lut_counter = AC_INTERVAL;
            }
        }

        t_end = get_micros();
        fps_frames++;

        if ((get_micros() - fps_start) >= 1000000) {
            uart_puts("FPS:");
            uart_putint(fps_frames);
            uart_puts(" frame=");
            uart_putint((int)(t_end - t_start));
            uart_puts("us\n");
            fps_frames = 0;
            fps_start = get_micros();
            keyboard_check_connect();
        }
    }
}

/* ---- QQVGA viewfinder loop (CAM2VRAM with HW 2× upscale) ---- */
static void viewfinder_qqvga(void)
{
    unsigned int fps_start;
    int fps_frames;
    unsigned int cam2vram_flags;

    fps_start = get_micros();
    fps_frames = 0;

    /* First frame: VSYNC-synced CAM2VRAM with upscale (no LUT yet) */
    cam2vram_flags = FPGC_DMA_CTRL_UPSCALE2X;
    if (display_mode == MODE_DITH)
        cam2vram_flags = cam2vram_flags | FPGC_DMA_CTRL_DITHER_EN;
    else if (display_mode == MODE_DITH8)
        cam2vram_flags = cam2vram_flags | FPGC_DMA_CTRL_DITHER_EN | FPGC_DMA_CTRL_DITHER_8;

    dma_start_cam2vram((unsigned int)FPGC_GPU_PIXEL_DATA,
                       QQVGA_BYTES, cam2vram_flags);
    while (dma_busy()) { }

    if (display_mode != MODE_RAW) {
        auto_contrast_from_hw();
    }

    while (1) {
        unsigned int t_start;
        unsigned int t_end;
        int key;

        t_start = get_micros();

        cam2vram_flags = FPGC_DMA_CTRL_UPSCALE2X;
        if (display_mode == MODE_DITH)
            cam2vram_flags = cam2vram_flags | FPGC_DMA_CTRL_DITHER_EN;
        else if (display_mode == MODE_DITH8)
            cam2vram_flags = cam2vram_flags | FPGC_DMA_CTRL_DITHER_EN | FPGC_DMA_CTRL_DITHER_8;
        if (ac_lut_valid && display_mode != MODE_RAW)
            cam2vram_flags = cam2vram_flags | FPGC_DMA_CTRL_LUT_EN;

        dma_start_cam2vram_immediate((unsigned int)FPGC_GPU_PIXEL_DATA,
                                     QQVGA_BYTES, cam2vram_flags);

        while (dma_busy()) {
            key = keyboard_poll();
            if (key == 'r' || key == 'R') {
                set_mode(MODE_RAW);
                auto_contrast_reset();
            } else if (key == 'd' || key == 'D') {
                set_mode(MODE_DITH);
                auto_contrast_reset();
            } else if (key == 'e' || key == 'E') {
                set_mode(MODE_DITH8);
                auto_contrast_reset();
            } else if (key == 'q' || key == 'Q') {
                while (dma_busy()) { }
                res_mode = RES_QVGA;
                return;
            }
        }

        if (display_mode != MODE_RAW) {
            ac_lut_counter = ac_lut_counter - 1;
            if (ac_lut_counter <= 0) {
                auto_contrast_from_hw();
                ac_lut_counter = AC_INTERVAL;
            }
        }

        t_end = get_micros();
        fps_frames++;

        if ((get_micros() - fps_start) >= 1000000) {
            uart_puts("FPS:");
            uart_putint(fps_frames);
            uart_puts(" frame=");
            uart_putint((int)(t_end - t_start));
            uart_puts("us [QQVGA]\n");
            fps_frames = 0;
            fps_start = get_micros();
            keyboard_check_connect();
        }
    }
}

void viewfinder_run(int initial_mode)
{
    int first;
    first = 1;
    res_mode = RES_QVGA;
    set_mode(initial_mode);

    while (1) {
        if (res_mode == RES_QQVGA) {
            uart_puts("Switching to QQVGA...\n");
            cam_disable();
            ov7670_set_qqvga();
            cam_enable_phase(1);
            while (!cam_frame_ready()) { }
            auto_contrast_reset();
            viewfinder_qqvga();
        } else {
            if (!first) {
                uart_puts("Switching to QVGA...\n");
                cam_disable();
                ov7670_set_qvga();
                cam_enable_phase(1);
                while (!cam_frame_ready()) { }
            }
            first = 0;
            auto_contrast_reset();
            viewfinder_qvga();
        }
    }
}
