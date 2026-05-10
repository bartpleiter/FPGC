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
#include "ch376.h"
#include "timer.h"
#include "settings.h"
#include "hud.h"
#include "storage.h"

/* Current display mode (shared with viewfinder.c) */
int display_mode = MODE_RAW;

/* ---- Keyboard input (bare-metal CH376 USB HID) ---- */
static int kb_spi;
static usb_device_info_t kb_dev;
static hid_keyboard_report_t kb_prev;
static int kb_connected;

static void keyboard_init(void)
{
    kb_spi = SPI_USB_0;
    kb_connected = 0;
    spi_deselect(SPI_FLASH_0);  /* clean SPI bus state */
    ch376_host_init(kb_spi);
}

/* Try to connect to a USB keyboard. Call periodically. */
void keyboard_check_connect(void)
{
    int st;
    st = ch376_test_connect(kb_spi);
    if (st == CH376_CONN_CONNECTED && !kb_connected) {
        /* Device just plugged in — wait for stabilization */
        delay(1000);
        if (ch376_enumerate_device(kb_spi, &kb_dev) == 1) {
            if (ch376_is_keyboard(&kb_dev)) {
                kb_connected = 1;
            }
        }
    } else if (st == CH376_CONN_DISCONNECTED && kb_connected) {
        kb_connected = 0;
        ch376_reset(kb_spi);
        ch376_host_init(kb_spi);
    }
}

/* Poll for a newly pressed key. Returns ASCII char or 0. */
int keyboard_poll(void)
{
    hid_keyboard_report_t report;
    int st;
    int i;
    int j;
    int keycode;
    int found;

    if (!kb_connected) return 0;
    if (ch376_test_connect(kb_spi) != CH376_CONN_READY) return 0;

    st = ch376_read_keyboard(kb_spi, &kb_dev, &report);
    if (st != 1) return 0;

    /* Find newly pressed key */
    for (i = 0; i < 6; i++) {
        keycode = report.keycode[i];
        if (keycode == 0) continue;
        found = 0;
        for (j = 0; j < 6; j++) {
            if (kb_prev.keycode[j] == keycode) found = 1;
        }
        if (!found) {
            kb_prev = report;
            return ch376_keycode_to_ascii(keycode, report.modifier);
        }
    }
    kb_prev = report;
    return 0;
}

/* Switch display mode and update palette */
void set_mode(int mode)
{
    display_mode = mode;
    if (mode == MODE_DITH) {
        setup_palette_4shade();
    } else if (mode == MODE_DITH8) {
        setup_palette_8shade();
    } else {
        setup_palette_greyscale();
    }
}

int main(void)
{
    int timeout;
    int status;

    /* Clear all VRAM planes (remove bootloader logo) */
    gpu_clear_vram();

    /* Load font patterns and palettes early (needed for format prompt) */
    hud_init();

    /* Load dither threshold/offset tables into DMA engine hardware */
    load_dither_tables();

    /* Init timer subsystem (required for delay() used by CH376) */
    timer_init();

    /* Init USB keyboard */
    keyboard_init();
    keyboard_check_connect();

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
            gpu_write_window_tile(17, 10, 'Y', PALETTE_GREEN_ON_BLACK);
            gpu_write_window_tile(18, 10, '/', PALETTE_GREEN_ON_BLACK);
            gpu_write_window_tile(19, 10, 'N', PALETTE_RED_ON_BLACK);
            gpu_write_window_tile(20, 10, ')', PALETTE_GREEN_ON_BLACK);

            /* Wait for Y or N keypress */
            while (1) {
                int fmt_key;
                fmt_key = keyboard_poll();
                if (fmt_key == 'y' || fmt_key == 'Y') {
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
                if (fmt_key == 'n' || fmt_key == 'N') {
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
