/*
 * ov7670_init.h — OV7670 register configuration via I2C
 *
 * Replaces the hardware CameraConfigure ROM sequence.
 * Call ov7670_init() before enabling camera capture.
 */
#ifndef OV7670_INIT_H
#define OV7670_INIT_H

/* OV7670 I2C device address (7-bit) */
#define OV7670_ADDR  0x21

/* Initialize OV7670 for QVGA YUV422 output.
 * Returns 0 on success, -1 on I2C error. */
int ov7670_init(void);

/* Switch to QQVGA (160×120) by enabling DCW downscaler.
 * Call after ov7670_init(). Returns 0 on success. */
int ov7670_set_qqvga(void);

/* Switch back to QVGA (320×240) by disabling DCW.
 * Returns 0 on success. */
int ov7670_set_qvga(void);

/* Reset AEC/AGC/night mode to auto defaults (for switching back to Auto).
 * Restores COM8=0xE7, COM11=0xE0, AEW/AEB defaults. */
void ov7670_reset_auto(void);

/* Set manual exposure mode: disable AEC/AGC, disable night mode,
 * reset exposure and frame timing to defaults. */
void ov7670_set_manual(void);

#endif /* OV7670_INIT_H */
