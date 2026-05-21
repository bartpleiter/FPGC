/*
 * ov7670_init.c — OV7670 QVGA YUV422 configuration via I2C
 *
 * This replaces the hardware CameraConfigure + OV7670_config_rom modules.
 * All register writes go through the generic I2C master.
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

/* Write an OV7670 register, returns 0 on success */
static int ov_write(int reg, int val)
{
    int rc;
    rc = i2c_write(OV7670_ADDR, reg, val);
    uart_putchar('.');  /* progress indicator */
    return rc;
}

int ov7670_init(void)
{
    int err;
    err = 0;

    /* Software reset */
    err |= ov_write(0x12, 0x80);
    delay_10ms();

    /* Output format: YUV422, QVGA (native 320x240) */
    err |= ov_write(0x12, 0x14);  /* COM7: QVGA mode, YUV output */
    err |= ov_write(0x6B, 0x4A);  /* DBLV: PLL x4 (25MHz × 4 = 100MHz internal) */
    delay_10ms();                  /* Let PLL lock */
    err |= ov_write(0x11, 0x00);  /* CLKRC: div-2 (silicon bug) → PCLK = 50MHz */
    err |= ov_write(0x0C, 0x00);  /* COM3: no DCW/scaling */
    err |= ov_write(0x3E, 0x00);  /* COM14: no manual PCLK scaling */
    err |= ov_write(0x70, 0x3A);  /* Scaling XSC */
    err |= ov_write(0x71, 0x35);  /* Scaling YSC */
    err |= ov_write(0x72, 0x11);  /* DCW control */
    err |= ov_write(0x73, 0xF0);  /* DCW control (Linux uses 0xF0) */
    err |= ov_write(0xA2, 0x02);  /* Scaling PCLK delay */

    /* Timing / window (from Linux kernel empirical values for QVGA) */
    err |= ov_write(0x17, 0x15);  /* HSTART = 168>>3 = 21 = 0x15 */
    err |= ov_write(0x18, 0x03);  /* HSTOP = 24>>3 = 3 */
    err |= ov_write(0x32, 0x80);  /* HREF: edge offset, low bits = 0 */
    err |= ov_write(0x19, 0x03);  /* VSTART = 12>>2 = 3 */
    err |= ov_write(0x1A, 0x7B);  /* VSTOP = 492>>2 = 123 */
    err |= ov_write(0x03, 0x0A);  /* VREF */

    /* YUV / data format */
    err |= ov_write(0x04, 0x00);  /* COM1: disable CCIR656 */
    err |= ov_write(0x40, 0xC0);  /* COM15: full output range [00..FF] */
    err |= ov_write(0x3A, 0x04);  /* TSLB: UYVY byte order, auto window */
    err |= ov_write(0x3D, 0xC0);  /* COM13: gamma enable, UV saturation auto */
    err |= ov_write(0x15, 0x00);  /* COM10: VSYNC positive, HREF positive */

    /* AGC / AEC */
    err |= ov_write(0x13, 0xE0);  /* COM8: fast AEC, step, banding filter ON */
    err |= ov_write(0x00, 0x00);  /* GAIN: 0 */
    err |= ov_write(0x10, 0x00);  /* AECH: exposure = 0 */
    err |= ov_write(0x0D, 0x40);  /* COM4 */
    err |= ov_write(0x14, 0x38);  /* COM9: max AGC 16x gain ceiling */
    err |= ov_write(0xA5, 0x05);  /* BD50MAX */
    err |= ov_write(0xAB, 0x07);  /* BD60MAX */
    err |= ov_write(0x24, 0x75);  /* AGC upper limit (was 0x95 — lower to preserve highlights) */
    err |= ov_write(0x25, 0x33);  /* AGC lower limit */
    err |= ov_write(0x26, 0xA5);  /* AGC/AEC fast mode (stable operating region) */
    err |= ov_write(0x9F, 0x78);  /* HAECC1 */
    err |= ov_write(0xA0, 0x68);  /* HAECC2 */
    err |= ov_write(0xA1, 0x03);  /* Magic */
    err |= ov_write(0xA6, 0xD8);  /* HAECC3 */
    err |= ov_write(0xA7, 0xD8);  /* HAECC4 */
    err |= ov_write(0xA8, 0xF0);  /* HAECC5 */
    err |= ov_write(0xA9, 0x90);  /* HAECC6 */
    err |= ov_write(0xAA, 0x94);  /* HAECC7 */
    err |= ov_write(0x13, 0xE7);  /* COM8: AGC + AEC + AWB all enabled */

    /* Gamma curve */
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

    /* Misc */
    err |= ov_write(0x0F, 0x4B);  /* COM6 (from Linux kernel defaults) */
    err |= ov_write(0x1E, 0x00);  /* MVFP: no mirror/flip */
    err |= ov_write(0x33, 0x0B);  /* CHLF */
    err |= ov_write(0x3C, 0x78);  /* COM12 */
    err |= ov_write(0x69, 0x00);  /* GFIX */
    err |= ov_write(0x74, 0x10);  /* REG74 (from Linux kernel) */
    err |= ov_write(0xB0, 0x84);  /* RSVD */
    err |= ov_write(0xB1, 0x0C);  /* ABLC1 */
    err |= ov_write(0xB2, 0x0E);  /* RSVD */
    err |= ov_write(0xB3, 0x82);  /* THL_ST (from Linux kernel, was 0x80) */

    /* Frame rate / banding — NO multi-frame exposure */
    err |= ov_write(0x3B, 0x10);  /* COM11: auto 50/60Hz detect, NO night mode */

    if (err) {
        uart_puts("OV7670: I2C errors during init\n");
        return -1;
    }

    uart_puts("OV7670: configured OK\n");
    return 0;
}
