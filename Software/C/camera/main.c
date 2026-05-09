/*
 * FPGC-Camera — Main Application
 *
 * Bare-metal camera app: displays a live viewfinder on HDMI using
 * the OV7670 sensor captured via the CameraCapture hardware.
 *
 * Display modes:
 *   RAW   — 256-shade greyscale (direct Y blit)
 *   DITH  — Game Boy style: downsample 2×2 → auto-contrast
 *           → 4×4 ordered dither (4 shades) → scale 2×
 *
 * Build: make compile-camera && make run-uart
 */
#include "cam_driver.h"
#include "image_proc.h"
#include "uart.h"
#include "dma.h"
#include "i2c.h"
#include "ov7670_init.h"
#include "gpu_hal.h"
#include "fpgc.h"
#include "sys.h"
#include "spi.h"
#include "ch376.h"
#include "timer.h"

/* Sensor dimensions (QVGA) */
#define SENS_W  320
#define SENS_H  240
#define SENS_BYTES (SENS_W * SENS_H)  /* 76800 */

/* Display dimensions */
#define DISP_W  320
#define DISP_H  240

/* Display modes */
#define MODE_RAW   0
#define MODE_DITH  1
#define MODE_DITH8 2

static int display_mode = MODE_RAW;

/* 4-shade Game Boy palette: black, dark grey, light grey, white */
static void setup_palette_4shade(void)
{
    unsigned int *pal;
    pal = (unsigned int *)FPGC_GPU_PIXEL_PALETTE;
    pal[0] = 0x000000;  /* black */
    pal[1] = 0x555555;  /* dark grey */
    pal[2] = 0xAAAAAA;  /* light grey */
    pal[3] = 0xFFFFFF;  /* white */
}

/* 8-shade evenly spaced greyscale palette */
static void setup_palette_8shade(void)
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
static void setup_palette_greyscale(void)
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

/* Blit raw 320×240 Y channel directly to VRAM */
static void blit_raw(unsigned int src_addr)
{
    unsigned char *buf;
    int i;

    buf = (unsigned char *)src_addr;
    for (i = 0; i < SENS_BYTES; i++) {
        __builtin_store(FPGC_GPU_PIXEL_DATA + i, (int)buf[i]);
    }
}

/* Auto-contrast LUT cached across frames */
static unsigned char ac_lut[256];
static int ac_lut_valid = 0;
static int ac_lut_counter = 0;

static void auto_contrast_cached(unsigned char *buf, int n)
{
    int i;
    int lo;
    int hi;
    int range;

    /* Recompute LUT every 8 frames (or first frame) */
    if (ac_lut_counter == 0 || !ac_lut_valid) {
        lo = 255;
        hi = 0;
        for (i = 0; i < n; i++) {
            if (buf[i] < lo) lo = buf[i];
            if (buf[i] > hi) hi = buf[i];
        }
        if (hi > lo) {
            range = hi - lo;
            for (i = 0; i < 256; i++) {
                if (i <= lo) ac_lut[i] = 0;
                else if (i >= hi) ac_lut[i] = 255;
                else ac_lut[i] = (unsigned char)(((i - lo) * 255) / range);
            }
        } else {
            for (i = 0; i < 256; i++) ac_lut[i] = (unsigned char)i;
        }
        ac_lut_valid = 1;
        ac_lut_counter = 8;
    }
    ac_lut_counter = ac_lut_counter - 1;

    /* Apply cached LUT */
    for (i = 0; i < n; i++) {
        buf[i] = ac_lut[buf[i]];
    }
}

/* Dither 320×240 directly to VRAM (full resolution, no downsample) */
static void blit_dithered(unsigned int src_addr)
{
    unsigned char *buf;
    int i;
    int x;
    int y;
    int mi;
    unsigned char p;

    buf = (unsigned char *)src_addr;

    init_dither_tables_ext();
    i = 0;
    for (y = 0; y < DISP_H; y++) {
        int y4;
        y4 = (y & 3) << 2;
        for (x = 0; x < DISP_W; x++) {
            int v;
            p = buf[i];
            mi = y4 + (x & 3);

            if (p < mat_dg_b_ext[mi]) {
                v = 0;
            } else if (p < mat_lg_dg_ext[mi]) {
                v = 1;
            } else if (p < mat_w_lg_ext[mi]) {
                v = 2;
            } else {
                v = 3;
            }

            __builtin_store(FPGC_GPU_PIXEL_DATA + i, v);
            i = i + 1;
        }
    }
}

/* Dither 320×240 to 8 shades directly to VRAM */
static void blit_dithered8(unsigned int src_addr)
{
    unsigned char *buf;
    int i;
    int x;
    int y;

    buf = (unsigned char *)src_addr;

    i = 0;
    for (y = 0; y < DISP_H; y++) {
        int y4;
        y4 = (y & 3) << 2;
        for (x = 0; x < DISP_W; x++) {
            int mi;
            int v;
            mi = y4 + (x & 3);
            v = ((int)buf[i] + bayer4_ext[mi] * 2 + 1) >> 5;
            if (v > 7) v = 7;
            __builtin_store(FPGC_GPU_PIXEL_DATA + i, v);
            i = i + 1;
        }
    }
}

/* Process one frame and display it */
static void process_frame(unsigned int src_addr, int do_print)
{
    unsigned char *buf;
    int i;

    /* Flush data cache so CPU reads fresh SDRAM data (DMA wrote it) */
    cache_flush_data();

    /* Print first 32 bytes for debug (only on selected frames) */
    if (do_print) {
        buf = (unsigned char *)src_addr;
        uart_puts("Px:");
        for (i = 0; i < 32; i++) {
            uart_putchar(' ');
            uart_puthex((unsigned int)buf[i], 0);
        }
        uart_putchar('\n');
    }

    if (display_mode == MODE_DITH) {
        blit_dithered(src_addr);
    } else if (display_mode == MODE_DITH8) {
        blit_dithered8(src_addr);
    } else {
        blit_raw(src_addr);
    }
}

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
static void keyboard_check_connect(void)
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
static int keyboard_poll(void)
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
static void set_mode(int mode)
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
    int frame_count;
    unsigned int frame_addr;
    unsigned int dma_dst;
    int timeout;
    int status;

    uart_puts("\n=== FPGC-Camera ===\n");
    uart_puts("Initializing...\n");

    /* Clear all VRAM planes (remove bootloader logo) */
    gpu_clear_vram();

    /* Default to raw greyscale mode */
    set_mode(MODE_RAW);

    /* Init timer subsystem (required for delay() used by CH376) */
    timer_init();

    /* Init USB keyboard */
    keyboard_init();
    keyboard_check_connect();  /* detect if already plugged in */

    /* Configure OV7670 via I2C */
    uart_puts("Configuring OV7670...\n");
    ov7670_init();

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

    frame_count = 0;

    /* FPS measurement */
    unsigned int fps_start;
    int fps_frames;

    fps_start = get_micros();
    fps_frames = 0;

    /* ---- Double-buffered pipelined capture ----
     * 1. First DMA waits for VSYNC (normal mode) to synchronize
     * 2. Subsequent DMAs use immediate mode: start capturing right away
     *    while the CPU blits the previous frame to VRAM
     * This gives ~1 frame period per iteration instead of ~2.
     */

    /* First frame: normal mode (waits for VSYNC sync) */
    dma_start_cam(CAM_BUF0_BYTE_ADDR, CAM_FRAME_BYTES);
    while (dma_busy()) { }

    /* Pipeline: start next capture before displaying previous */
    int cur_buf;
    unsigned int buf_addr[2];
    unsigned int done_addr;

    buf_addr[0] = CAM_BUF0_BYTE_ADDR;
    buf_addr[1] = CAM_BUF1_BYTE_ADDR;
    cur_buf = 1;  /* Next capture goes to buf 1 */
    done_addr = CAM_BUF0_BYTE_ADDR;  /* First frame is in buf 0 */

    while (1) {
        unsigned int t_start;
        unsigned int t_blit_end;
        unsigned int t_cap_end;

        t_start = get_micros();

        /* Start next DMA immediately (skip frame_done wait) */
        dma_start_cam_immediate(buf_addr[cur_buf], CAM_FRAME_BYTES);

        /* Blit the previously completed frame while DMA captures */
        process_frame(done_addr, (frame_count < 5));
        t_blit_end = get_micros();

        /* Wait for DMA if still running */
        while (dma_busy()) { }
        t_cap_end = get_micros();

        /* Swap: the just-completed capture is the next frame to display */
        done_addr = buf_addr[cur_buf];
        cur_buf = 1 - cur_buf;

        frame_count++;
        fps_frames++;

        /* Print FPS and timing every second */
        if ((get_micros() - fps_start) >= 1000000) {
            uart_puts("FPS:");
            uart_putint(fps_frames);
            uart_puts(" blit=");
            uart_putint((int)(t_blit_end - t_start));
            uart_puts("us total=");
            uart_putint((int)(t_cap_end - t_start));
            uart_puts("us\n");
            fps_frames = 0;
            fps_start = get_micros();

            /* Periodically check for keyboard connect/disconnect */
            keyboard_check_connect();
        }

        /* Poll keyboard for mode switch */
        {
            int key;
            key = keyboard_poll();
            if (key == 'r' || key == 'R') {
                set_mode(MODE_RAW);
            } else if (key == 'd' || key == 'D') {
                set_mode(MODE_DITH);
            } else if (key == 'e' || key == 'E') {
                set_mode(MODE_DITH8);
            }
        }
    }

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
