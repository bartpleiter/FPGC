/*
 * ov7670_init.c - OV7670 QVGA YUV422 configuration via I2C
 *
 * V2: Adapted from FPGA_OV7670_Camera_Interface reference project
 * (proven 30fps VGA), modified for YUV QVGA output.
 */
#include "ov7670_init.h"
#include "i2c.h"
#include "uart.h"
#include "fpgc.h"

/* Simple delay (~10ms at 100MHz) */
static void delay_10ms(void)
{
    int i;
    for (i = 0; i < 500000; i++) {
        __builtin_load(FPGC_I2C_CMD);  /* prevent optimization */
    }
}

/* Write an OV7670 register. Returns 0 on success, -1 on I2C error. */
static int ov_write(int reg, int val)
{
    int rc;

    rc = i2c_write(OV7670_ADDR, reg, val);
    if (rc) {
        uart_puts("W!");
        return -1;
    }

    uart_putchar('.');
    return 0;
}

int ov7670_init(void)
{
    int err;
    err = 0;

    /* Software reset */
    err |= ov_write(0x12, 0x80);
    delay_10ms();

    /* ---- Output format ---- */
    err |= ov_write(0x12, 0x10);  /* COM7: QVGA + YUV */
    err |= ov_write(0x11, 0x80);  /* CLKRC: internal clock, no prescaler */
    err |= ov_write(0x0C, 0x00);  /* COM3: no DCW/scaling */
    err |= ov_write(0x3E, 0x00);  /* COM14: no manual PCLK scaling */
    err |= ov_write(0x70, 0x3A);  /* Scaling XSC */
    err |= ov_write(0x71, 0x35);  /* Scaling YSC */
    err |= ov_write(0x72, 0x11);  /* DCW control */
    err |= ov_write(0x73, 0xF0);  /* DCW control */
    err |= ov_write(0xA2, 0x02);  /* Scaling PCLK delay */

    /* ---- Timing / window (from reference project) ---- */
    err |= ov_write(0x17, 0x15);  /* HSTART */
    err |= ov_write(0x18, 0x03);  /* HSTOP */
    err |= ov_write(0x32, 0x80);  /* HREF: edge offset */
    err |= ov_write(0x19, 0x03);  /* VSTART */
    err |= ov_write(0x1A, 0x7B);  /* VSTOP */
    err |= ov_write(0x03, 0x0A);  /* VREF */

    /* ---- YUV / data format ---- */
    err |= ov_write(0x04, 0x00);  /* COM1: disable CCIR656 */
    err |= ov_write(0x40, 0xC0);  /* COM15: full output range [00..FF] */
    err |= ov_write(0x3A, 0x04);  /* TSLB: auto window */
    err |= ov_write(0x3D, 0xC0);  /* COM13: gamma enable, UV sat auto */
    err |= ov_write(0x15, 0x00);  /* COM10: VSYNC/HREF positive */

    /* ---- AGC / AEC (from reference project) ---- */
    err |= ov_write(0x13, 0xE0);  /* COM8: fast AEC, step, banding ON */
    err |= ov_write(0x00, 0x00);  /* GAIN: 0 */
    err |= ov_write(0x10, 0x00);  /* AECH: exposure = 0 */
    err |= ov_write(0x0D, 0x40);  /* COM4 */
    err |= ov_write(0x14, 0x18);  /* COM9: 4x gain ceiling (reference value) */
    err |= ov_write(0xA5, 0x05);  /* BD50MAX */
    err |= ov_write(0xAB, 0x07);  /* BD60MAX */
    err |= ov_write(0x24, 0x95);  /* AEW: upper limit */
    err |= ov_write(0x25, 0x33);  /* AEB: lower limit */
    err |= ov_write(0x26, 0xE3);  /* VPT: fast mode */
    err |= ov_write(0x9F, 0x78);  /* HAECC1 */
    err |= ov_write(0xA0, 0x68);  /* HAECC2 */
    err |= ov_write(0xA1, 0x03);  /* Magic */
    err |= ov_write(0xA6, 0xD8);  /* HAECC3 */
    err |= ov_write(0xA7, 0xD8);  /* HAECC4 */
    err |= ov_write(0xA8, 0xF0);  /* HAECC5 */
    err |= ov_write(0xA9, 0x90);  /* HAECC6 */
    err |= ov_write(0xAA, 0x94);  /* HAECC7 */
    err |= ov_write(0x13, 0xE7);  /* COM8: AGC + AEC + AWB enabled */

    /* ---- Night mode ---- */
    err |= ov_write(0x3B, 0xE0);  /* COM11: night mode ON, 1/8 frame rate min */

    /* ---- Gamma curve ---- */
    err |= ov_write(0x7A, 0x20);
    err |= ov_write(0x7B, 0x10);
    err |= ov_write(0x7C, 0x1E);
    err |= ov_write(0x7D, 0x35);
    err |= ov_write(0x7E, 0x5A);
    err |= ov_write(0x7F, 0x69);
    err |= ov_write(0x80, 0x76);
    err |= ov_write(0x81, 0x80);
    err |= ov_write(0x82, 0x88);
    err |= ov_write(0x83, 0x8F);
    err |= ov_write(0x84, 0x96);
    err |= ov_write(0x85, 0xA3);
    err |= ov_write(0x86, 0xAF);
    err |= ov_write(0x87, 0xC4);
    err |= ov_write(0x88, 0xD7);
    err |= ov_write(0x89, 0xE8);

    /* ---- Misc (from reference project) ---- */
    err |= ov_write(0x0F, 0x41);  /* COM6: reset timings */
    err |= ov_write(0x1E, 0x00);  /* MVFP: no mirror/flip */
    err |= ov_write(0x33, 0x0B);  /* CHLF */
    err |= ov_write(0x3C, 0x78);  /* COM12 */
    err |= ov_write(0x69, 0x00);  /* GFIX */
    err |= ov_write(0x74, 0x00);  /* REG74: no digital gain */
    err |= ov_write(0xB0, 0x84);  /* RSVD */
    err |= ov_write(0xB1, 0x0C);  /* ABLC1 */
    err |= ov_write(0xB2, 0x0E);  /* RSVD */
    err |= ov_write(0xB3, 0x80);  /* THL_ST */

    if (err) {
        uart_puts("\nOV7670: errors during init\n");
        return -1;
    }

    uart_puts("\nOV7670: configured OK\n");
    return 0;
}
