/*
 * cam_driver.c — FPGC-Camera hardware driver implementation
 */
#include "cam_driver.h"

void cam_enable(void)
{
    __builtin_store(FPGC_CAM_CTRL, 1);
}

void cam_disable(void)
{
    __builtin_store(FPGC_CAM_CTRL, 0);
}

int cam_frame_ready(void)
{
    int status;
    status = __builtin_load(FPGC_CAM_STATUS);
    return status & 1;  /* bit 0 = frame_done_latch (read-clear) */
}

int cam_last_buffer(void)
{
    /* CAM_STATUS[1] = current_buf (the buffer being WRITTEN to).
     * The last completed frame is in the OTHER buffer. */
    int status;
    status = __builtin_load(FPGC_CAM_STATUS);
    /* Note: reading STATUS clears frame_done_latch, so only call
     * this after cam_frame_ready() returned 1, or accept the clear. */
    return (status >> 1) & 1;
}

unsigned int cam_last_frame_addr(void)
{
    int current;
    current = cam_last_buffer();
    /* The camera is writing to current_buf. The COMPLETED frame is
     * in the opposite buffer. */
    if (current)
        return CAM_BUF0_BYTE_ADDR;
    return CAM_BUF1_BYTE_ADDR;
}

void cam_sccb_write(int reg_addr, int data)
{
    int val;
    int ready;
    /* Wait for SCCB master to be ready */
    ready = __builtin_load(FPGC_CAM_SCCB);
    while (!(ready & 1))
    {
        ready = __builtin_load(FPGC_CAM_SCCB);
    }
    /* Write: {[15:8] reg_addr, [7:0] data} */
    val = ((reg_addr & 0xFF) << 8) | (data & 0xFF);
    __builtin_store(FPGC_CAM_SCCB, val);
}
