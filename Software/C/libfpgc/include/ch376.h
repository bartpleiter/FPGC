/*
 * ch376.h — CH376 USB Host Controller driver for B32P3/FPGC.
 *
 * Placeholder header. Full port from Software/C/libs/kernel/io/ch376.c
 * will be done during Phase 3 (BDOS kernel migration).
 *
 * Key change from original: ch376_get_top_nint() and ch376_get_bottom_nint()
 * use hwio_read() instead of inline asm.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef FPGC_CH376_H
#define FPGC_CH376_H

/* USB Host SPI bus assignments */
#define CH376_SPI_TOP    SPI_USB_0
#define CH376_SPI_BOTTOM SPI_USB_1

/* TODO: Full CH376 API will be ported during Phase 3.
 * See Software/C/libs/kernel/io/ch376.h for current API. */

#endif /* FPGC_CH376_H */
