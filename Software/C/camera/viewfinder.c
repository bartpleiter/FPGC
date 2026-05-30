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
#include "gpu_hal.h"
#include "storage.h"
#include "bmp.h"
#include "gallery.h"
#include "gpu_data_ascii.h"
#include "menu.h"

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

void blit_dithered_dma(unsigned int src_addr)
{
    unsigned int flags;
    flags = FPGC_DMA_CTRL_DITHER_EN;
    cache_flush_data();
    dma_start_mem2vram_ex(
        (unsigned int)FPGC_GPU_PIXEL_DATA, src_addr,
        (unsigned int)SENS_BYTES, flags);
    while (dma_busy()) { }
}

void blit_dithered8_dma(unsigned int src_addr)
{
    unsigned int flags;
    flags = FPGC_DMA_CTRL_DITHER_EN | FPGC_DMA_CTRL_DITHER_8;
    cache_flush_data();
    dma_start_mem2vram_ex(
        (unsigned int)FPGC_GPU_PIXEL_DATA, src_addr,
        (unsigned int)SENS_BYTES, flags);
    while (dma_busy()) { }
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

/* Average luminance from hardware pixel sum (-1 = not yet computed) */
static int avg_lum = -1;

/* SDRAM buffer for captured frames (below BRFS cache at 0x2800000) */
#define CAPTURE_BUF_ADDR  0x2600000

/* Pixel counts for avg luminance computation */
#define QVGA_PIXELS   76800   /* 320 * 240 */
#define QQVGA_PIXELS  19200   /* 160 * 120 */

/* Update avg_lum from the hardware pixel sum register.
 * Called on HUD update frames (every HUD_INTERVAL). */
static void update_avg_lum(unsigned int pixel_count)
{
    unsigned int psum;
    psum = dma_pixel_sum();
    avg_lum = (int)(psum / pixel_count);
    if (avg_lum > 255)
        avg_lum = 255;
}

/* Build filename: /DCIM/IMG_NNNN.BMP — REMOVED, now uses storage_next_image() */

/* ---- Viewfinder loop (forward declarations) ---- */

/* These are provided by main.c */
extern int display_mode;
extern void set_mode(int mode);
extern int keyboard_poll(void);
extern void keyboard_check_connect(void);
extern int keyboard_fn1_held(void);
extern int keyboard_fn2_held(void);

/* Current resolution mode */
int res_mode = RES_QVGA;

/* ---- Quick-adjust state ---- */
/* Order matches visual left-to-right HUD layout */
#define QA_MODE        0
#define QA_RES         1
#define QA_SHUTTER     2
#define QA_EXPOSURE    3
#define QA_ISO         4
#define QA_BRIGHTNESS  5
#define QA_CONTRAST    6
#define QA_GAMMA       7
#define QA_COUNT       8

static int qa_current = QA_SHUTTER;
static int qa_highlight_timer = 0;
#define QA_HIGHLIGHT_FRAMES  60  /* ~2 seconds at 30fps */

/* Flag: next DMA needs VSYNC re-sync (set after cam_disable) */
static int need_resync;

static void qa_cycle(int direction)
{
    qa_current = qa_current + direction;
    if (qa_current < 0) qa_current = QA_COUNT - 1;
    if (qa_current >= QA_COUNT) qa_current = 0;
    qa_highlight_timer = QA_HIGHLIGHT_FRAMES;
}

/* Adjust parameter and return pending_action code:
 *   0 = no action needed
 *   1 = resolution switch (exit loop)
 *   2 = mode switch (stop cam, reconfigure, restart)
 *   3 = shutter/ISO changed (stop cam, apply, restart)
 *   5 = brightness/contrast/gamma (apply via LUT, no cam restart)
 */
static int qa_adjust(int param, int direction)
{
    qa_highlight_timer = QA_HIGHLIGHT_FRAMES;

    switch (param) {
    case QA_MODE:
        /* Cycle display mode: GREY(0) → DITH(1) → DITH8(2) */
        display_mode = display_mode + direction;
        if (display_mode < 0) display_mode = 2;
        if (display_mode > 2) display_mode = 0;
        return 2;
    case QA_RES:
        /* Toggle resolution */
        if (res_mode == RES_QVGA)
            res_mode = RES_QQVGA;
        else
            res_mode = RES_QVGA;
        return 1;
    case QA_SHUTTER:
        settings_adjust_shutter(direction);
        return 3;
    case QA_EXPOSURE:
        settings_adjust_exposure(direction);
        return 3;
    case QA_ISO:
        settings_adjust_iso(direction);
        return 3;
    case QA_BRIGHTNESS:
        settings_adjust_brightness(direction);
        return 5;
    case QA_CONTRAST:
        settings_adjust_contrast(direction);
        return 5;
    case QA_GAMMA:
        settings_adjust_gamma(direction);
        return 5;
    }
    return 0;
}

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

    /* Apply display processing (dithering) to match what user sees */
    if (display_mode == MODE_DITH || display_mode == MODE_DITH8) {
        unsigned char *raw;
        unsigned char *proc;
        int pixels;
        int i;
        raw = (unsigned char *)CAPTURE_BUF_ADDR;
        proc = (unsigned char *)PROCESS_BUF_ADDR;
        pixels = cap_w * cap_h;

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
        /* RAW mode — save directly */
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
    hud_update(last_fps, qa_current, qa_highlight_timer, avg_lum);
}

/* HUD update interval (frames) */
#define HUD_INTERVAL 5

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
static int menu_reopen = 0;  /* re-open menu after resolution switch */

static int handle_key(int key)
{
    if (key == 0) return 0;

    /* When menu is open, disable camera during redraws to avoid desync */
    if (menu_is_open()) {
        if (key == BTN_MENU) {
            while (dma_busy()) { }
            cam_disable();
            menu_close();
            hud_update(last_fps, qa_current, qa_highlight_timer, avg_lum);
            need_resync = 1;
            return 0;
        }
        /* Disable camera for any menu key (draw is slow) */
        while (dma_busy()) { }
        cam_disable();
        pending_action = menu_handle_key(key);
        need_resync = 1;
        if (pending_action == 6) {
            /* Resolution change — set reopen flag, close menu, exit loop */
            menu_reopen = 1;
            menu_close();
            if (res_mode == RES_QVGA) res_mode = RES_QQVGA;
            else res_mode = RES_QVGA;
            update_remaining();
            return 1;
        }
        if (pending_action == 7) {
            /* Gallery entry */
            pending_action = 0;
            menu_close();
            return 3;
        }
        return 0;
    }

    /* ---- Viewfinder mode (menu closed) ---- */

    /* Shutter */
    if (key == BTN_SHUTTER) {
        capture_pending = 1;
    }
    /* Menu open — disable camera to avoid DMA bus contention during draw */
    else if (key == BTN_MENU) {
        while (dma_busy()) { }
        cam_disable();
        menu_open();
        need_resync = 1;
    }
    /* Left/Right: cycle quick-adjust parameter */
    else if (key == BTN_LEFT) {
        qa_cycle(-1);
        hud_update(last_fps, qa_current, qa_highlight_timer, avg_lum);
    }
    else if (key == BTN_RIGHT) {
        qa_cycle(1);
        hud_update(last_fps, qa_current, qa_highlight_timer, avg_lum);
    }
    /* Up/Down: adjust current parameter or held shortcut */
    else if (key == BTN_UP || key == BTN_DOWN) {
        int dir;
        int act;
        dir = (key == BTN_UP) ? 1 : -1;

        if (keyboard_fn1_held() && keyboard_fn2_held()) {
            /* Fn1+Fn2 held: exposure */
            act = qa_adjust(QA_EXPOSURE, dir);
        } else if (keyboard_fn1_held()) {
            /* Fn1 held: brightness */
            act = qa_adjust(QA_BRIGHTNESS, dir);
        } else if (keyboard_fn2_held()) {
            /* Fn2 held: contrast */
            act = qa_adjust(QA_CONTRAST, dir);
        } else {
            /* No modifier: adjust current quick-adjust param */
            act = qa_adjust(qa_current, dir);
        }
        /* Resolution switch exits the loop immediately */
        if (act == 1) {
            update_remaining();
            return 1;
        }
        pending_action = act;
        hud_update(last_fps, qa_current, qa_highlight_timer, avg_lum);
    }
    /* HUD toggle: Fn1 alone (no arrow) */
    else if (key == BTN_FN1) {
        /* Fn1 without arrow press: no action (used as modifier) */
    }
    else if (key == BTN_FN2) {
        /* Fn2 without arrow press: no action (used as modifier) */
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

    /* Display mode change — palette reload only, no camera restart */
    if (action == 2) {
        if (display_mode == MODE_DITH)
            setup_palette_4shade();
        else if (display_mode == MODE_DITH8)
            setup_palette_8shade();
        else
            setup_palette_greyscale();
        return;
    }

    /* Stop camera capture for clean I2C access */
    cam_disable();

    switch (action) {
    case 3:  /* Shutter/ISO/Exposure change */
        settings_apply_shutter();
        settings_apply_exposure();
        settings_apply_iso();
        break;
    case 4:  /* Full reset */
        settings_init();
        break;
    case 5:  /* brightness/contrast/orientation/sharpness/gamma */
        settings_apply_brightness();
        settings_apply_contrast();
        settings_apply_orientation();
        settings_apply_sharpness();
        settings_apply_gamma();
        break;
    }

    /* Restart camera capture and wait for a clean frame */
    cam_enable_phase(1);
    while (!cam_frame_ready()) { }
    hud_update(last_fps, qa_current, qa_highlight_timer, avg_lum);
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

    cam2vram_flags = 0;
    if (display_mode == MODE_DITH)
        cam2vram_flags = FPGC_DMA_CTRL_DITHER_EN;
    else if (display_mode == MODE_DITH8)
        cam2vram_flags = FPGC_DMA_CTRL_DITHER_EN | FPGC_DMA_CTRL_DITHER_8;

    /* Draw initial HUD while no DMA is running (no bus contention) */
    hud_update(last_fps, qa_current, qa_highlight_timer, avg_lum);

    /* Re-open menu if we came from a resolution switch */
    if (menu_reopen) {
        menu_reopen = 0;
        cam_disable();
        menu_open();
        need_resync = 1;
    }

    /* First frame always needs VSYNC sync */
    need_resync = 1;

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

        /* Start DMA — use VSYNC-synced version after cam was disabled */
        if (need_resync) {
            cam_enable_phase(1);
            while (!cam_frame_ready()) { }
            dma_start_cam2vram((unsigned int)FPGC_GPU_PIXEL_DATA,
                               CAM_FRAME_BYTES, cam2vram_flags);
            need_resync = 0;
        } else {
            dma_start_cam2vram_immediate((unsigned int)FPGC_GPU_PIXEL_DATA,
                                         CAM_FRAME_BYTES, cam2vram_flags);
        }

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

        t_end = get_micros();
        fps_frames++;

        /* Quick-adjust highlight countdown */
        if (qa_highlight_timer > 0)
            qa_highlight_timer = qa_highlight_timer - 1;

        /* Periodic HUD update */
        hud_counter = hud_counter + 1;
        if (hud_counter >= HUD_INTERVAL) {
            update_avg_lum(QVGA_PIXELS);
            hud_update(last_fps, qa_current, qa_highlight_timer, avg_lum);
            hud_counter = 0;
        }

        if ((get_micros() - fps_start) >= 1000000) {
            last_fps = fps_frames;
            fps_frames = 0;
            fps_start = get_micros();
            /* USB keyboard init can take 50ms+ — disable camera to avoid desync */
            cam_disable();
            keyboard_check_connect();
            need_resync = 1;
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

    cam2vram_flags = FPGC_DMA_CTRL_UPSCALE2X;
    if (display_mode == MODE_DITH)
        cam2vram_flags = cam2vram_flags | FPGC_DMA_CTRL_DITHER_EN;
    else if (display_mode == MODE_DITH8)
        cam2vram_flags = cam2vram_flags | FPGC_DMA_CTRL_DITHER_EN | FPGC_DMA_CTRL_DITHER_8;

    /* Draw initial HUD while no DMA is running (no bus contention) */
    hud_update(last_fps, qa_current, qa_highlight_timer, avg_lum);

    /* Re-open menu if we came from a resolution switch */
    if (menu_reopen) {
        menu_reopen = 0;
        cam_disable();
        menu_open();
        need_resync = 1;
    }

    /* First frame always needs VSYNC sync */
    need_resync = 1;

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

        /* Start DMA — use VSYNC-synced version after cam was disabled */
        if (need_resync) {
            cam_enable_phase(1);
            while (!cam_frame_ready()) { }
            dma_start_cam2vram((unsigned int)FPGC_GPU_PIXEL_DATA,
                               QQVGA_BYTES, cam2vram_flags);
            need_resync = 0;
        } else {
            dma_start_cam2vram_immediate((unsigned int)FPGC_GPU_PIXEL_DATA,
                                         QQVGA_BYTES, cam2vram_flags);
        }

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

        t_end = get_micros();
        fps_frames++;

        /* Quick-adjust highlight countdown */
        if (qa_highlight_timer > 0)
            qa_highlight_timer = qa_highlight_timer - 1;

        /* Periodic HUD update */
        hud_counter = hud_counter + 1;
        if (hud_counter >= HUD_INTERVAL) {
            update_avg_lum(QQVGA_PIXELS);
            hud_update(last_fps, qa_current, qa_highlight_timer, avg_lum);
            hud_counter = 0;
        }

        if ((get_micros() - fps_start) >= 1000000) {
            last_fps = fps_frames;
            fps_frames = 0;
            fps_start = get_micros();
            /* USB keyboard init can take 50ms+ — disable camera to avoid desync */
            cam_disable();
            keyboard_check_connect();
            need_resync = 1;
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

    /* Clear splash screen */
    gpu_clear_window();

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
            reason = viewfinder_qvga();
        }

        /* Handle gallery mode */
        if (reason == 3) {
            while (dma_busy()) { }
            cam_disable();
            gallery_run();
            /* Restore viewfinder state — camera re-init happens at loop top */
            update_remaining();
            set_mode(display_mode);
            hud_init();
        }
        /* reason == 1: resolution switch, just loops back */
    }
}
