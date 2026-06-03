/*
 * FPGC-Camera — Main Application
 *
 * Bare-metal camera app: displays a live viewfinder on HDMI using
 * the OV7670 sensor captured via the CameraCapture hardware.
 *
 * Display modes:
 *   RAW   — 256-shade greyscale (direct Y blit)
 *   DITH  — 4-shade Dashboy Camera dither
 *   DITH8 — 8-shade Bayer ordered dither
 *
 * Build: make compile-camera && make run-uart
 */
#include "viewfinder.h"
#include "cam_driver.h"
#include "ov7670_init.h"
#include "gpu_hal.h"
#include "gpu_data_ascii.h"
#include "fpgc.h"
#include "sys.h"
#include "spi.h"
#include "timer.h"
#include "settings.h"
#include "hud.h"
#include "storage.h"

/* Current display mode (shared with viewfinder.c) */
int display_mode = MODE_RAW;

int state_prev = 0;

/* Poll for a newly pressed key. Returns ASCII char or 0. */
int keyboard_poll(void)
{
    int state;
    state = __builtin_load(FPGC_BTN_STATE);
    if (state != state_prev) {
        int bit0_changed = (state ^ state_prev) & 0x1; // Up
        int bit1_changed = (state ^ state_prev) & 0x2; // Down
        int bit2_changed = (state ^ state_prev) & 0x4; // Left
        int bit3_changed = (state ^ state_prev) & 0x8; // Right
        int bit4_changed = (state ^ state_prev) & 0x10; // Shutter
        int bit5_changed = (state ^ state_prev) & 0x20; // Menu

        state_prev = state;

        // If any of the bits changed from 0 to 1, return the corresponding key
        if (bit0_changed && (state & 0x1)) return BTN_UP;
        if (bit1_changed && (state & 0x2)) return BTN_DOWN;
        if (bit2_changed && (state & 0x4)) return BTN_LEFT;
        if (bit3_changed && (state & 0x8)) return BTN_RIGHT;
        if (bit4_changed && (state & 0x10)) return BTN_SHUTTER;
        if (bit5_changed && (state & 0x20)) return BTN_MENU;
    }
    return 0;
}

/* HID keycodes for Fn buttons */
#define HID_KEY_U  0x18
#define HID_KEY_O  0x12

/* Check if Fn1 (U) is currently held */
int keyboard_fn1_held(void)
{
    // Currently disabled
    return 0;
}

/* Check if Fn2 (O) is currently held */
int keyboard_fn2_held(void)
{
    // Currently disabled
    return 0;
}

/* Switch display mode and update palette */
void set_mode(int mode)
{
    /* Save current brightness/contrast to the mode we're leaving */
    cam_settings.mode_presets[display_mode].brightness =
        cam_settings.brightness;
    cam_settings.mode_presets[display_mode].contrast =
        cam_settings.contrast;

    display_mode = mode;

    /* Load brightness/contrast from the new mode's preset */
    cam_settings.brightness =
        cam_settings.mode_presets[mode].brightness;
    cam_settings.contrast =
        cam_settings.mode_presets[mode].contrast;

    /* Apply palette for the display mode */
    if (mode == MODE_DITH) {
        setup_palette_4shade();
    } else if (mode == MODE_DITH8) {
        setup_palette_8shade();
    } else {
        setup_palette_greyscale();
    }

    /* Apply the loaded brightness/contrast to sensor */
    settings_apply_brightness();
    settings_apply_contrast();
}

int main(void)
{
    int timeout;
    int status;

    /* Clear all VRAM planes (remove bootloader logo) */
    gpu_clear_vram();

    /* Load font patterns and palettes early (needed for format prompt) */
    hud_init();

    /* Show splash screen during init */
    hud_splash("FPGC-Camera");

    /* Load dither threshold/offset tables into DMA engine hardware */
    load_dither_tables();

    /* Init timer subsystem (required for delay() used by CH376) */
    timer_init();

    /* Initialize SD card and BRFS filesystem */
    {
        int sd_rc;
        sd_rc = storage_init();
        if (sd_rc == 1) {

            /* SD card found but no BRFS — ask user to format */
            gpu_write_window_tile(5, 10, 'F', PALETTE_WHITE_ON_BLACK);
            gpu_write_window_tile(6, 10, 'o', PALETTE_WHITE_ON_BLACK);
            gpu_write_window_tile(7, 10, 'r', PALETTE_WHITE_ON_BLACK);
            gpu_write_window_tile(8, 10, 'm', PALETTE_WHITE_ON_BLACK);
            gpu_write_window_tile(9, 10, 'a', PALETTE_WHITE_ON_BLACK);
            gpu_write_window_tile(10, 10, 't', PALETTE_WHITE_ON_BLACK);
            gpu_write_window_tile(12, 10, 'S', PALETTE_WHITE_ON_BLACK);
            gpu_write_window_tile(13, 10, 'D', PALETTE_WHITE_ON_BLACK);
            gpu_write_window_tile(14, 10, '?', PALETTE_WHITE_ON_BLACK);
            gpu_write_window_tile(16, 10, '(', PALETTE_GREEN_ON_BLACK);
            gpu_write_window_tile(17, 10, 'S', PALETTE_GREEN_ON_BLACK);
            gpu_write_window_tile(18, 10, '/', PALETTE_GREEN_ON_BLACK);
            gpu_write_window_tile(19, 10, 'M', PALETTE_RED_ON_BLACK);
            gpu_write_window_tile(20, 10, ')', PALETTE_GREEN_ON_BLACK);

            /* Wait for Y or N keypress */
            while (1) {
                int fmt_key;
                fmt_key = keyboard_poll();
                if (fmt_key == BTN_SHUTTER) {
                    /* Show formatting message */
                    gpu_clear_window();
                    gpu_write_window_tile(5, 12, 'F', PALETTE_YELLOW_ON_BLACK);
                    gpu_write_window_tile(6, 12, 'o', PALETTE_YELLOW_ON_BLACK);
                    gpu_write_window_tile(7, 12, 'r', PALETTE_YELLOW_ON_BLACK);
                    gpu_write_window_tile(8, 12, 'm', PALETTE_YELLOW_ON_BLACK);
                    gpu_write_window_tile(9, 12, 'a', PALETTE_YELLOW_ON_BLACK);
                    gpu_write_window_tile(10, 12, 't', PALETTE_YELLOW_ON_BLACK);
                    gpu_write_window_tile(11, 12, 't', PALETTE_YELLOW_ON_BLACK);
                    gpu_write_window_tile(12, 12, 'i', PALETTE_YELLOW_ON_BLACK);
                    gpu_write_window_tile(13, 12, 'n', PALETTE_YELLOW_ON_BLACK);
                    gpu_write_window_tile(14, 12, 'g', PALETTE_YELLOW_ON_BLACK);
                    gpu_write_window_tile(15, 12, '.', PALETTE_YELLOW_ON_BLACK);
                    gpu_write_window_tile(16, 12, '.', PALETTE_YELLOW_ON_BLACK);
                    gpu_write_window_tile(17, 12, '.', PALETTE_YELLOW_ON_BLACK);
                    storage_format();
                    break;
                }
                if (fmt_key == BTN_MENU) {
                    break;
                }
            }
            gpu_clear_window();
        }
        /* sd_rc == -1 means no SD card — storage_ready will be 0 */
    }

    /* Configure OV7670 via I2C */
    ov7670_init();

    /* Initialize camera settings (Auto mode, defaults) */
    settings_init();

    /* Enable camera capture with even byte phase (Y in YUYV) */
    cam_enable_phase(1);

    /* Wait for first frame_done so we know VSYNC boundary */
    timeout = 0;
    while (1) {
        status = __builtin_load(FPGC_CAM_STATUS);
        if (status & 1) {
            break;
        }
        timeout = timeout + 1;
        if (timeout >= 10000000) {
            cam_disable();
            while (1) { /* halt */ }
        }
    }

    /* Enter viewfinder loop (does not return) */
    viewfinder_run(MODE_RAW);

    return 0;
}

/* Required by crt0_baremetal -- interrupt handler for timer subsystem */
void interrupt(void)
{
    int id;
    id = get_int_id();
    if (id == FPGC_INTID_TIMER1) {
        timer_isr_handler(TIMER_1);
    } else if (id == FPGC_INTID_TIMER2) {
        timer_isr_handler(TIMER_2);
    }
}
