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

#endif /* OV7670_INIT_H */
