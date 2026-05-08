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

    /* Flush L1 data cache so CPU sees what camera DMA wrote to SDRAM */
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

    /* Blit raw Y bytes directly to VRAMPX — no processing */
    dma_blit_to_vram(FPGC_GPU_PIXEL_DATA, src_addr, CAM_FRAME_BYTES);
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

    /* Enable camera capture */
    uart_puts("Enabling camera capture...\n");
    cam_enable();

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

    /* Main viewfinder loop: DMA-based capture */
    while (1) {
        /* Alternate between two SDRAM buffers */
        if (frame_count & 1)
            dma_dst = CAM_BUF1_BYTE_ADDR;
        else
            dma_dst = CAM_BUF0_BYTE_ADDR;

        /* Start DMA in CAM2MEM mode — DMA engine waits for VSYNC internally */
        dma_start_cam(dma_dst, CAM_FRAME_BYTES);

        /* Poll DMA until transfer is complete */
        while (dma_busy()) { }

        /* Process and display the captured frame */
        process_frame(dma_dst, (frame_count & 63) == 0);

        frame_count++;

        /* Print frame count every 64 frames */
        if ((frame_count & 63) == 0) {
            uart_puts("Frame ");
            uart_putint(frame_count);
            uart_putchar('\n');
        }
    }

    return 0;
}

/* Required by crt0_baremetal — empty interrupt handler */
void interrupt(void)
{
}
