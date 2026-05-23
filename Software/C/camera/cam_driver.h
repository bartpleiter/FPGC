/*
 * cam_driver.h — FPGC-Camera hardware driver
 *
 * MMIO register wrappers for the OV7670 camera subsystem.
 */
#ifndef CAM_DRIVER_H
#define CAM_DRIVER_H

/* Camera MMIO registers */
#define FPGC_CAM_CTRL      0x1C000088
#define FPGC_CAM_STATUS    0x1C00008C

/* Default frame buffer byte addresses (line_addr << 5) */
#define CAM_BUF0_BYTE_ADDR  0x02000000
#define CAM_BUF1_BYTE_ADDR  0x02012C00

/* Frame dimensions */
#define CAM_FRAME_W  320
#define CAM_FRAME_H  240
#define CAM_FRAME_BYTES  (CAM_FRAME_W * CAM_FRAME_H)  /* 76800 */

/* Enable/disable continuous capture */
void cam_enable(void);
void cam_enable_phase(int phase);  /* phase: 0=odd bytes, 1=even bytes */
void cam_disable(void);

/* Poll for new frame; returns 1 if a new frame is ready (clears the flag) */
int cam_frame_ready(void);

/* Returns which buffer holds the LAST completed frame (0 or 1).
 * The camera is currently writing to the OTHER buffer. */
int cam_last_buffer(void);

/* Returns byte address of the last completed frame buffer */
unsigned int cam_last_frame_addr(void);

/* Read raw CAM_CTRL register */
int cam_read_ctrl(void);

#endif /* CAM_DRIVER_H */
