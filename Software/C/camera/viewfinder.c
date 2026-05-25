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
#include "dma.h"
#include "fpgc.h"
#include "sys.h"
#include "cam_driver.h"
#include "ov7670_init.h"
#include "settings.h"
#include "hud.h"
#include "storage.h"
#include "bmp.h"
#include "gallery.h"
#include "gpu_data_ascii.h"

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

/* ---- Capture: save current frame to BRFS ---- */

static int next_image_num = 0;

/* Last measured FPS (for HUD) — declared early for do_capture */
static int last_fps = 0;

/* SDRAM buffer for captured frames (below BRFS cache at 0x2800000) */
#define CAPTURE_BUF_ADDR  0x2600000

/* Build filename: /DCIM/IMG_NNNN.BMP — REMOVED, now uses storage_next_image() */

/* ---- Viewfinder loop (forward declarations) ---- */

/* These are provided by main.c */
extern int display_mode;
extern void set_mode(int mode);
extern int keyboard_poll(void);
extern void keyboard_check_connect(void);

/* Current resolution mode */
static int res_mode = RES_QVGA;

/* Cached remaining image count (updated on capture/delete/res change) */
static int cached_remaining = 0;

static void update_remaining(void)
{
    cached_remaining = storage_remaining_images(res_mode);
}

/*
 * Capture the current frame: stop cam, save BMP, flash border, sync.
 * Called from handle_key when space is pressed.
 */
static int capture_pending = 0;

/* Second SDRAM buffer for processed (dithered/LUT) pixels */
#define PROCESS_BUF_ADDR  0x2700000

/* Map 4-shade dither indices to greyscale values */
static unsigned char shade4_lut[4] = { 0, 85, 170, 255 };

/* Map 8-shade dither indices to greyscale values */
static unsigned char shade8_lut[8] = { 0, 36, 73, 109, 146, 182, 219, 255 };

static void do_capture(void)
{
    char path[40];
    int rc;
    int cap_bytes;
    int cap_w;
    int cap_h;

    if (!storage_ready) return;

    /* Wait for current DMA to finish */
    while (dma_busy()) { }

    /* Stop current camera-to-VRAM stream */
    cam_disable();

    /* Show "Saving..." on window layer */
    {
        unsigned int *tile;
        int i;
        const char *msg;
        msg = "Saving image...";
        tile = (unsigned int *)FPGC_GPU_WIN_TILE_TABLE;
        for (i = 0; msg[i] != 0; i++) {
            tile[12 * 40 + 12 + i] = (unsigned int)msg[i];
        }
    }

    /* Determine capture size based on resolution mode */
    if (res_mode == RES_QQVGA) {
        cap_bytes = QQVGA_BYTES;     /* 160×120 = 19200 */
        cap_w = QQVGA_W;
        cap_h = QQVGA_H;
    } else {
        cap_bytes = CAM_FRAME_BYTES; /* 320×240 = 76800 */
        cap_w = BMP_WIDTH;
        cap_h = BMP_HEIGHT;
    }

    /* Capture one frame from camera to SDRAM via DMA CAM2MEM */
    cam_enable_phase(1);
    dma_start_cam((unsigned int)CAPTURE_BUF_ADDR, (unsigned int)cap_bytes);
    while (dma_busy()) { }
    cam_disable();

    /* Apply display processing (LUT + dithering) to match what user sees */
    if (display_mode == MODE_DITH || display_mode == MODE_DITH8) {
        unsigned char *raw;
        unsigned char *proc;
        int pixels;
        int i;
        raw = (unsigned char *)CAPTURE_BUF_ADDR;
        proc = (unsigned char *)PROCESS_BUF_ADDR;
        pixels = cap_w * cap_h;

        /* Apply auto-contrast LUT first if enabled */
        if (cam_settings.auto_contrast && ac_lut_valid) {
            for (i = 0; i < pixels; i++) {
                raw[i] = ac_lut[raw[i]];
            }
        }

        /* Apply dithering (outputs palette indices) */
        if (display_mode == MODE_DITH) {
            dither_4x4(raw, proc, cap_w, cap_h);
            /* Map indices 0-3 to greyscale values */
            for (i = 0; i < pixels; i++) {
                proc[i] = shade4_lut[proc[i]];
            }
        } else {
            dither_8shade(raw, proc, cap_w, cap_h);
            /* Map indices 0-7 to greyscale values */
            for (i = 0; i < pixels; i++) {
                proc[i] = shade8_lut[proc[i]];
            }
        }

        /* Save from processed buffer */
        next_image_num = storage_next_image(path, 40);
        if (next_image_num < 0) { cam_enable_phase(1); return; }
        rc = bmp_save(&cam_brfs, path, (unsigned int)PROCESS_BUF_ADDR,
                      cap_w, cap_h);
    } else {
        /* RAW mode — apply only auto-contrast if enabled */
        if (cam_settings.auto_contrast && ac_lut_valid) {
            unsigned char *raw;
            int pixels;
            int i;
            raw = (unsigned char *)CAPTURE_BUF_ADDR;
            pixels = cap_w * cap_h;
            for (i = 0; i < pixels; i++) {
                raw[i] = ac_lut[raw[i]];
            }
        }

        next_image_num = storage_next_image(path, 40);
        if (next_image_num < 0) { cam_enable_phase(1); return; }
        rc = bmp_save(&cam_brfs, path, (unsigned int)CAPTURE_BUF_ADDR,
                      cap_w, cap_h);
    }

    if (rc == 0) {
        /* Sync to SD card */
        storage_sync();
        update_remaining();
    }

    /* Clear "Saving..." message */
    {
        unsigned int *tile;
        int i;
        tile = (unsigned int *)FPGC_GPU_WIN_TILE_TABLE;
        for (i = 0; i < 15; i++) {
            tile[12 * 40 + 12 + i] = 0;
        }
    }

    /* Resume camera capture and wait for a clean frame */
    cam_enable_phase(1);
    while (!cam_frame_ready()) { }

    /* Update HUD */
    hud_update(last_fps, cached_remaining);
}

/* HUD update interval (frames) */
#define HUD_INTERVAL 5

#define AC_INTERVAL 8

/*
 * Handle a single keypress. Returns:
 *   0 = normal (continue)
 *   1 = resolution switch requested (caller should exit loop)
 *   2 = mode switch (need to stop cam, reconfigure, restart)
 *   3 = shutter/ISO changed (need to stop cam, apply, restart)
 *   4 = settings reset (stop cam, full reinit)
 *
 * Keys that require I2C writes set a pending flag — the viewfinder
 * loop applies them after the current DMA completes with the camera
 * stopped. This is a design convention for clean frame boundaries.
 */
static int pending_action = 0;

static int handle_key(int key)
{
    if (key == 0) return 0;

    /* Display mode keys (palette only, no I2C) */
    if (key == 'r' || key == 'R') {
        set_mode(MODE_RAW);
        auto_contrast_reset();
    } else if (key == 'd' || key == 'D') {
        set_mode(MODE_DITH);
        auto_contrast_reset();
    } else if (key == 'e' || key == 'E') {
        set_mode(MODE_DITH8);
        auto_contrast_reset();
    }
    /* Resolution toggle */
    else if (key == 'q' || key == 'Q') {
        while (dma_busy()) { }
        if (res_mode == RES_QVGA) res_mode = RES_QQVGA;
        else res_mode = RES_QVGA;
        update_remaining();
        return 1;
    }
    /* Shooting mode cycle — deferred to after DMA */
    else if (key == 'm' || key == 'M') {
        pending_action = 2;
    }
    /* Shutter speed (Manual only, deferred) */
    else if (key == '[') {
        if (cam_settings.shoot_mode == SHOOT_M) {
            cam_settings.shutter = cam_settings.shutter - 1;
            if (cam_settings.shutter < 0) cam_settings.shutter = 0;
            pending_action = 3;
        }
    } else if (key == ']') {
        if (cam_settings.shoot_mode == SHOOT_M) {
            cam_settings.shutter = cam_settings.shutter + 1;
            if (cam_settings.shutter >= SHUTTER_COUNT)
                cam_settings.shutter = SHUTTER_COUNT - 1;
            pending_action = 3;
        }
    }
    /* ISO / gain (Manual only, deferred) */
    else if (key == '-') {
        if (cam_settings.shoot_mode == SHOOT_M) {
            cam_settings.iso = cam_settings.iso - 1;
            if (cam_settings.iso < 0) cam_settings.iso = 0;
            pending_action = 3;
        }
    } else if (key == '=') {
        if (cam_settings.shoot_mode == SHOOT_M) {
            cam_settings.iso = cam_settings.iso + 1;
            if (cam_settings.iso >= ISO_COUNT)
                cam_settings.iso = ISO_COUNT - 1;
            pending_action = 3;
        }
    }
    /* Exposure (Manual only, deferred) */
    else if (key == '{') {
        if (cam_settings.shoot_mode == SHOOT_M) {
            cam_settings.exposure = cam_settings.exposure - 1;
            if (cam_settings.exposure < 0) cam_settings.exposure = 0;
            pending_action = 3;
        }
    } else if (key == '}') {
        if (cam_settings.shoot_mode == SHOOT_M) {
            cam_settings.exposure = cam_settings.exposure + 1;
            if (cam_settings.exposure >= EXPOSURE_COUNT)
                cam_settings.exposure = EXPOSURE_COUNT - 1;
            pending_action = 3;
        }
    }
    /* Brightness (deferred) */
    else if (key == '9') {
        cam_settings.brightness = cam_settings.brightness - 16;
        if (cam_settings.brightness < -128) cam_settings.brightness = -128;
        pending_action = 5;
    } else if (key == '0') {
        cam_settings.brightness = cam_settings.brightness + 16;
        if (cam_settings.brightness > 127) cam_settings.brightness = 127;
        pending_action = 5;
    }
    /* Contrast (deferred) */
    else if (key == '7') {
        cam_settings.contrast = cam_settings.contrast - 8;
        if (cam_settings.contrast < 0) cam_settings.contrast = 0;
        pending_action = 5;
    } else if (key == '8') {
        cam_settings.contrast = cam_settings.contrast + 8;
        if (cam_settings.contrast > 127) cam_settings.contrast = 127;
        pending_action = 5;
    }
    /* Mirror / Flip (deferred) */
    else if (key == 'x' || key == 'X') {
        cam_settings.mirror = !cam_settings.mirror;
        pending_action = 5;
    } else if (key == 'y' || key == 'Y') {
        cam_settings.flip = !cam_settings.flip;
        pending_action = 5;
    }
    /* HUD toggle (no I2C needed) */
    else if (key == 'h' || key == 'H') {
        settings_toggle_hud();
        hud_update(last_fps, cached_remaining);
    }
    /* Auto-contrast LUT toggle */
    else if (key == 'l' || key == 'L') {
        settings_toggle_auto_contrast();
        auto_contrast_reset();
    }
    /* Reset all settings to defaults (deferred) */
    else if (key == '`' || key == '~') {
        pending_action = 4;
    }
    /* Capture (space bar) — deferred */
    else if (key == ' ') {
        capture_pending = 1;
    }
    /* Gallery viewer */
    else if (key == 'g' || key == 'G') {
        return 3;  /* signal gallery entry */
    }

    return 0;
}

/*
 * Apply pending I2C actions after DMA is idle.
 * Stops camera, applies settings, restarts camera.
 */
static void apply_pending(void)
{
    int action;
    action = pending_action;
    pending_action = 0;
    if (action == 0) return;

    /* Stop camera capture for clean I2C access */
    cam_disable();

    switch (action) {
    case 2:  /* Mode switch */
        if (cam_settings.shoot_mode == SHOOT_AUTO)
            cam_settings.shoot_mode = SHOOT_M;
        else
            cam_settings.shoot_mode = SHOOT_AUTO;
        settings_apply_mode();
        break;
    case 3:  /* Shutter/ISO/Exposure change */
        settings_apply_shutter();
        settings_apply_exposure();
        settings_apply_iso();
        break;
    case 4:  /* Full reset */
        settings_init();
        break;
    case 5:  /* brightness/contrast/orientation */
        settings_apply_brightness();
        settings_apply_contrast();
        settings_apply_orientation();
        break;
    }

    /* Restart camera capture and wait for a clean frame */
    cam_enable_phase(1);
    while (!cam_frame_ready()) { }
    hud_update(last_fps, cached_remaining);
    auto_contrast_reset();
}

/* ---- QVGA viewfinder loop (CAM2VRAM direct) ---- */
/* Returns: 1=resolution switch, 3=gallery */
static int viewfinder_qvga(void)
{
    unsigned int fps_start;
    int fps_frames;
    int hud_counter;
    unsigned int cam2vram_flags;

    fps_start = get_micros();
    fps_frames = 0;
    hud_counter = 0;

    /* First frame: VSYNC-synced CAM2VRAM (no LUT yet) */
    cam2vram_flags = 0;
    if (display_mode == MODE_DITH)
        cam2vram_flags = FPGC_DMA_CTRL_DITHER_EN;
    else if (display_mode == MODE_DITH8)
        cam2vram_flags = FPGC_DMA_CTRL_DITHER_EN | FPGC_DMA_CTRL_DITHER_8;

    dma_start_cam2vram((unsigned int)FPGC_GPU_PIXEL_DATA,
                       CAM_FRAME_BYTES, cam2vram_flags);
    while (dma_busy()) { }

    if (display_mode != MODE_RAW && cam_settings.auto_contrast) {
        auto_contrast_from_hw();
    }

    hud_update(last_fps, cached_remaining);

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
        if (ac_lut_valid && display_mode != MODE_RAW
            && cam_settings.auto_contrast)
            cam2vram_flags = cam2vram_flags | FPGC_DMA_CTRL_LUT_EN;

        dma_start_cam2vram_immediate((unsigned int)FPGC_GPU_PIXEL_DATA,
                                     CAM_FRAME_BYTES, cam2vram_flags);

        while (dma_busy()) {
            int hk;
            key = keyboard_poll();
            hk = handle_key(key);
            if (hk) return hk;
        }

        /* Handle capture after DMA completes */
        if (capture_pending) {
            capture_pending = 0;
            do_capture();
        }

        /* Apply any deferred I2C settings (cam stop/start) */
        apply_pending();

        if (display_mode != MODE_RAW
            && cam_settings.auto_contrast) {
            ac_lut_counter = ac_lut_counter - 1;
            if (ac_lut_counter <= 0) {
                auto_contrast_from_hw();
                ac_lut_counter = AC_INTERVAL;
            }
        }

        t_end = get_micros();
        fps_frames++;

        /* Periodic HUD update */
        hud_counter = hud_counter + 1;
        if (hud_counter >= HUD_INTERVAL) {
            hud_update(last_fps, cached_remaining);
            hud_counter = 0;
        }

        if ((get_micros() - fps_start) >= 1000000) {
            last_fps = fps_frames;
            fps_frames = 0;
            fps_start = get_micros();
            keyboard_check_connect();
        }
    }
}

/* ---- QQVGA viewfinder loop (CAM2VRAM with HW 2× upscale) ---- */
/* Returns: 1=resolution switch, 3=gallery */
static int viewfinder_qqvga(void)
{
    unsigned int fps_start;
    int fps_frames;
    int hud_counter;
    unsigned int cam2vram_flags;

    fps_start = get_micros();
    fps_frames = 0;
    hud_counter = 0;

    /* First frame: VSYNC-synced CAM2VRAM with upscale (no LUT yet) */
    cam2vram_flags = FPGC_DMA_CTRL_UPSCALE2X;
    if (display_mode == MODE_DITH)
        cam2vram_flags = cam2vram_flags | FPGC_DMA_CTRL_DITHER_EN;
    else if (display_mode == MODE_DITH8)
        cam2vram_flags = cam2vram_flags | FPGC_DMA_CTRL_DITHER_EN | FPGC_DMA_CTRL_DITHER_8;

    dma_start_cam2vram((unsigned int)FPGC_GPU_PIXEL_DATA,
                       QQVGA_BYTES, cam2vram_flags);
    while (dma_busy()) { }

    if (display_mode != MODE_RAW && cam_settings.auto_contrast) {
        auto_contrast_from_hw();
    }

    hud_update(last_fps, cached_remaining);

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
        if (ac_lut_valid && display_mode != MODE_RAW
            && cam_settings.auto_contrast)
            cam2vram_flags = cam2vram_flags | FPGC_DMA_CTRL_LUT_EN;

        dma_start_cam2vram_immediate((unsigned int)FPGC_GPU_PIXEL_DATA,
                                     QQVGA_BYTES, cam2vram_flags);

        while (dma_busy()) {
            int hk;
            key = keyboard_poll();
            hk = handle_key(key);
            if (hk) return hk;
        }

        /* Handle capture after DMA completes */
        if (capture_pending) {
            capture_pending = 0;
            do_capture();
        }

        /* Apply any deferred I2C settings (cam stop/start) */
        apply_pending();

        if (display_mode != MODE_RAW
            && cam_settings.auto_contrast) {
            ac_lut_counter = ac_lut_counter - 1;
            if (ac_lut_counter <= 0) {
                auto_contrast_from_hw();
                ac_lut_counter = AC_INTERVAL;
            }
        }

        t_end = get_micros();
        fps_frames++;

        /* Periodic HUD update */
        hud_counter = hud_counter + 1;
        if (hud_counter >= HUD_INTERVAL) {
            hud_update(last_fps, cached_remaining);
            hud_counter = 0;
        }

        if ((get_micros() - fps_start) >= 1000000) {
            last_fps = fps_frames;
            fps_frames = 0;
            fps_start = get_micros();
            keyboard_check_connect();
        }
    }
}

void viewfinder_run(int initial_mode)
{
    int first;
    int reason;
    first = 1;
    res_mode = RES_QVGA;
    set_mode(initial_mode);

    /* Image counter is loaded by storage_init() */

    update_remaining();

    while (1) {
        if (res_mode == RES_QQVGA) {
            cam_disable();
            ov7670_set_qqvga();
            /* Re-apply mode overlays (init_mode wiped them) */
            settings_reapply();
            cam_enable_phase(1);
            while (!cam_frame_ready()) { }
            auto_contrast_reset();
            reason = viewfinder_qqvga();
        } else {
            if (!first) {
                cam_disable();
                ov7670_set_qvga();
                /* Re-apply mode overlays (init_mode wiped them) */
                settings_reapply();
                cam_enable_phase(1);
                while (!cam_frame_ready()) { }
            }
            first = 0;
            auto_contrast_reset();
            reason = viewfinder_qvga();
        }

        /* Handle gallery mode */
        if (reason == 3) {
            while (dma_busy()) { }
            cam_disable();
            gallery_run();
            /* Restore viewfinder: re-init camera for current res */
            update_remaining();
            set_mode(display_mode);
            hud_init();
            if (res_mode == RES_QQVGA) {
                ov7670_set_qqvga();
            } else {
                ov7670_set_qvga();
            }
            settings_reapply();
            cam_enable_phase(1);
            while (!cam_frame_ready()) { }
            auto_contrast_reset();
            first = 0;
        }
        /* reason == 1: resolution switch, just loops back */
    }
}
