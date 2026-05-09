/*
 * FPGC-Camera — Main Application
 *
 * Bare-metal camera app: displays a live dithered viewfinder on HDMI
 * using the OV7670 sensor captured via the CameraCapture hardware.
 *
 * Pipeline: Y channel from SDRAM → downsample 2×2 → auto-contrast
 *         → 4×4 ordered dither → scale 2× → VRAMPX framebuffer
 *
 * Build: make compile-camera && make run-uart
 */
#include "cam_driver.h"
#include "image_proc.h"
#include "uart.h"
#include "dma.h"
#include "i2c.h"
#include "ov7670_init.h"
#include "fpgc.h"
#include "sys.h"

/* Display dimensions */
#define DISP_W  320
#define DISP_H  240

/* Processing dimensions (after downsample) */
#define PROC_W  160
#define PROC_H  120

/* Working buffers in SDRAM — placed after kernel region */
#define BUF_DS_ADDR     0x00500000  /* downsample output: 160×120 = 19200 bytes */
#define BUF_DITH_ADDR   0x00505000  /* dither output: 160×120 = 19200 bytes */
#define BUF_DISP_ADDR   0x0050A000  /* display output: 320×240 = 76800 bytes */

/* 4-shade palette: black, dark grey, light grey, white */
static void setup_palette(void)
{
    unsigned int *pal;
    int i;
    unsigned int grey;
    pal = (unsigned int *)FPGC_GPU_PIXEL_PALETTE;
    /* Full 256-shade greyscale ramp for raw debug display */
    for (i = 0; i < 256; i++) {
        grey = (unsigned int)i;
        pal[i] = (grey << 16) | (grey << 8) | grey;
    }
}

/* Process one frame and display it — RAW DEBUG MODE */
static void process_frame(unsigned int src_addr, int do_print)
{
    unsigned char *buf;
    int i;

    /* Flush data cache so CPU reads fresh SDRAM data (DMA wrote it) */
    cache_flush_data();

    buf = (unsigned char *)src_addr;

    /* Print first 32 bytes for debug (only on selected frames) */
    if (do_print) {
        uart_puts("Px:");
        for (i = 0; i < 32; i++) {
            uart_putchar(' ');
            uart_puthex((unsigned int)buf[i], 0);
        }
        uart_putchar('\n');
    }

    /* CPU-based blit to VRAM — doesn't use DMA, so DMA stays free
     * for the concurrent camera capture. */
    for (i = 0; i < CAM_FRAME_BYTES; i++) {
        __builtin_store(FPGC_GPU_PIXEL_DATA + i, (int)buf[i]);
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

    /* Set up 256-shade greyscale palette */
    setup_palette();

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
        }
    }

    return 0;
}

/* Required by crt0_baremetal -- empty interrupt handler */
void interrupt(void)
{
}
