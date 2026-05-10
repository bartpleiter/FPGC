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

/* Blit raw 320×240 Y channel to VRAM via DMA */
static void blit_raw_dma(unsigned int src_addr)
{
    cache_flush_data();
    dma_start_mem2vram_ex(
        (unsigned int)FPGC_GPU_PIXEL_DATA, src_addr,
        (unsigned int)SENS_BYTES, 0);
    while (dma_busy()) { }
}

/* Blit with auto-contrast LUT via DMA (LUT must be pre-loaded) */
static void blit_lut_dma(unsigned int src_addr)
{
    cache_flush_data();
    dma_start_mem2vram_ex(
        (unsigned int)FPGC_GPU_PIXEL_DATA, src_addr,
        (unsigned int)SENS_BYTES,
        FPGC_DMA_CTRL_LUT_EN);
    while (dma_busy()) { }
}

/* Blit with 4-shade dithering via DMA (thresholds must be pre-loaded) */
static void blit_dithered_dma(unsigned int src_addr, int use_lut)
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

/* Blit with 8-shade Bayer dithering via DMA */
static void blit_dithered8_dma(unsigned int src_addr, int use_lut)
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

/* Auto-contrast LUT cached across frames */
static unsigned char ac_lut[256];
static int ac_lut_valid = 0;
static int ac_lut_counter = 0;

/* Compute auto-contrast LUT from an SDRAM buffer and upload to DMA engine.
 * Only recomputes every 8 frames.  Returns 1 if LUT is valid (loaded). */
static int auto_contrast_update(unsigned char *buf, int n)
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
        /* Upload LUT to DMA engine */
        for (i = 0; i < 256; i++) {
            dma_lut_write(i, (int)ac_lut[i]);
        }
        ac_lut_valid = 1;
        ac_lut_counter = 8;
    }
    ac_lut_counter = ac_lut_counter - 1;
    return ac_lut_valid;
}

/* Load dither threshold and Bayer offset tables into DMA engine hardware.
 * Call once at startup. */
static void load_dither_tables(void)
{
    int mi;

    /* Initialize software dither tables so we can read them */
    init_dither_tables_ext();

    /* Upload 4-shade thresholds: 3 tables × 16 matrix positions */
    for (mi = 0; mi < 16; mi++) {
        dma_dither_thresh_write(0, mi, (int)mat_dg_b_ext[mi]);
        dma_dither_thresh_write(1, mi, (int)mat_lg_dg_ext[mi]);
        dma_dither_thresh_write(2, mi, (int)mat_w_lg_ext[mi]);
    }
    /* Upload 8-shade Bayer offsets */
    for (mi = 0; mi < 16; mi++) {
        dma_dither_bayer_write(mi, (int)bayer4_ext[mi]);
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

    /* Load dither threshold/offset tables into DMA engine hardware */
    load_dither_tables();

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

    /* ---- Sequential DMA: blit → capture ----
     * DMA is single-channel, so VRAM blit and camera capture are sequential.
     * The DMA blit is ~2ms (vs ~5ms CPU blit for RAW, ~60ms for dither),
     * so sequential gives ~28fps for all modes.
     *
     * Auto-contrast LUT computation (CPU reads from SDRAM) runs during the
     * DMA capture wait period — it reads from the PREVIOUS frame buffer
     * (not being overwritten by DMA), so there's no conflict. The LUT
     * computed from frame N is used to display frame N+1 (imperceptible lag).
     */

    /* First frame: normal mode (waits for VSYNC sync) */
    dma_start_cam(CAM_BUF0_BYTE_ADDR, CAM_FRAME_BYTES);
    while (dma_busy()) { }

    int cur_buf;
    int prev_buf;
    unsigned int buf_addr[2];

    buf_addr[0] = CAM_BUF0_BYTE_ADDR;
    buf_addr[1] = CAM_BUF1_BYTE_ADDR;
    cur_buf = 0;  /* First captured frame is in buf 0 */

    while (1) {
        unsigned int t_start;
        unsigned int t_blit_end;
        unsigned int t_total_end;

        t_start = get_micros();

        /* 1. DMA blit the just-captured frame to VRAM (~2ms) */
        cache_flush_data();
        if (frame_count < 5) {
            unsigned char *buf;
            int i;
            buf = (unsigned char *)buf_addr[cur_buf];
            uart_puts("Px:");
            for (i = 0; i < 32; i++) {
                uart_putchar(' ');
                uart_puthex((unsigned int)buf[i], 0);
            }
            uart_putchar('\n');
        }
        if (display_mode == MODE_DITH) {
            blit_dithered_dma(buf_addr[cur_buf], ac_lut_valid);
        } else if (display_mode == MODE_DITH8) {
            blit_dithered8_dma(buf_addr[cur_buf], ac_lut_valid);
        } else {
            blit_raw_dma(buf_addr[cur_buf]);
        }
        t_blit_end = get_micros();

        /* 2. Start capture into other buffer */
        prev_buf = cur_buf;
        cur_buf = 1 - cur_buf;
        dma_start_cam_immediate(buf_addr[cur_buf], CAM_FRAME_BYTES);

        /* 3. During capture: compute AC LUT from the just-displayed frame.
         *    Reads buf[prev_buf] which DMA is NOT writing to. */
        if (display_mode != MODE_RAW) {
            auto_contrast_update((unsigned char *)buf_addr[prev_buf],
                                 SENS_BYTES);
        }

        /* 4. Wait for capture and poll keyboard */
        while (dma_busy()) {
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
        t_total_end = get_micros();

        frame_count++;
        fps_frames++;

        /* Print FPS and timing every second */
        if ((get_micros() - fps_start) >= 1000000) {
            uart_puts("FPS:");
            uart_putint(fps_frames);
            uart_puts(" blit=");
            uart_putint((int)(t_blit_end - t_start));
            uart_puts("us total=");
            uart_putint((int)(t_total_end - t_start));
            uart_puts("us\n");
            fps_frames = 0;
            fps_start = get_micros();

            /* Periodically check for keyboard connect/disconnect */
            keyboard_check_connect();
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
