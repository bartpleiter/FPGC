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
    pal = (unsigned int *)FPGC_GPU_PIXEL_PALETTE;
    pal[0] = 0x000000;  /* shade 0: black */
    pal[1] = 0x555555;  /* shade 1: dark grey */
    pal[2] = 0xAAAAAA;  /* shade 2: light grey */
    pal[3] = 0xFFFFFF;  /* shade 3: white */
}

/* Process one frame and display it */
static void process_frame(unsigned int src_addr)
{
    unsigned char *src;
    unsigned char *ds_buf;
    unsigned char *dith_buf;
    unsigned char *disp_buf;

    src      = (unsigned char *)src_addr;
    ds_buf   = (unsigned char *)BUF_DS_ADDR;
    dith_buf = (unsigned char *)BUF_DITH_ADDR;
    disp_buf = (unsigned char *)BUF_DISP_ADDR;

    /* Flush L1 data cache so CPU sees what camera DMA wrote to SDRAM */
    cache_flush_data();

    /* 320×240 → 160×120 */
    downsample_2x2(src, ds_buf, CAM_FRAME_W, CAM_FRAME_H);

    /* Auto-contrast stretch (in-place on downsample buffer) */
    auto_contrast(ds_buf, PROC_W, PROC_H);

    /* 4×4 ordered dither → 2-bit shades (0-3) */
    dither_4x4(ds_buf, dith_buf, PROC_W, PROC_H);

    /* 2× upscale → 320×240 display buffer */
    scale_2x(dith_buf, disp_buf, PROC_W, PROC_H);

    /* Flush cache before DMA reads the display buffer */
    cache_flush_data();

    /* DMA blit to VRAMPX (320×240 bytes, 32-byte aligned) */
    dma_blit_to_vram(FPGC_GPU_PIXEL_DATA, BUF_DISP_ADDR, DISP_W * DISP_H);
}

int main(void)
{
    int frame_count;
    unsigned int frame_addr;

    uart_puts("\n=== FPGC-Camera ===\n");
    uart_puts("Initializing...\n");

    /* Set up 4-shade greyscale palette */
    setup_palette();

    /* Enable camera capture (CameraConfigure auto-inits OV7670 on first enable) */
    uart_puts("Enabling camera capture...\n");
    cam_enable();

    uart_puts("Waiting for first frame...\n");

    frame_count = 0;

    /* Main viewfinder loop */
    while (1) {
        /* Wait for a new frame from the camera */
        while (!cam_frame_ready()) {
            /* spin */
        }

        /* Get the address of the completed frame buffer */
        frame_addr = cam_last_frame_addr();

        /* Process and display */
        process_frame(frame_addr);

        frame_count++;

        /* Print frame count every 60 frames (~1 second at 60 fps) */
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
