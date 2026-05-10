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
#include "uart.h"
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
                uart_puts("KB connected\n");
            }
        }
    } else if (st == CH376_CONN_DISCONNECTED && kb_connected) {
        kb_connected = 0;
        uart_puts("KB disconnected\n");
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
        uart_puts("Mode: DITH4\n");
    } else if (mode == MODE_DITH8) {
        setup_palette_8shade();
        uart_puts("Mode: DITH8\n");
    } else {
        setup_palette_greyscale();
        uart_puts("Mode: RAW\n");
    }
}

int main(void)
{
    int timeout;
    int status;

    uart_puts("\n=== FPGC-Camera ===\n");
    uart_puts("Initializing...\n");

    /* Clear all VRAM planes (remove bootloader logo) */
    gpu_clear_vram();

    /* Load dither threshold/offset tables into DMA engine hardware */
    load_dither_tables();

    /* Init timer subsystem (required for delay() used by CH376) */
    timer_init();

    /* Init USB keyboard */
    keyboard_init();
    keyboard_check_connect();

    /* Configure OV7670 via I2C */
    uart_puts("Configuring OV7670...\n");
    ov7670_init();

    /* Initialize camera settings (Auto mode, defaults) */
    settings_init();

    /* Initialize HUD overlay */
    hud_init();

    /* Enable camera capture with even byte phase (Y in YUYV) */
    uart_puts("Enabling camera capture (phase=1: even bytes)...\n");
    cam_enable_phase(1);

    uart_puts("Waiting for first frame_done...\n");

    /* Wait for first frame_done so we know VSYNC boundary */
    timeout = 0;
    while (1) {
        status = __builtin_load(FPGC_CAM_STATUS);
        if (status & 1) {
            uart_puts("First frame_done received!\n");
            break;
        }
        timeout = timeout + 1;
        if ((timeout & 0x1FFFF) == 0) {
            uart_putchar('.');
        }
        if (timeout >= 10000000) {
            uart_puts("\nTIMEOUT: no frame. DBG=");
            uart_putint(__builtin_load(0x1C00009C));
            uart_putchar('\n');
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
