/*
 * enc28j60.h — ENC28J60 Ethernet Controller driver for B32P3/FPGC.
 *
 * Placeholder header. Full port from Software/C/libs/kernel/io/enc28j60.c
 * will be done during Phase 3 (BDOS kernel migration).
 *
 * This driver is pure C (no inline asm, no volatile I/O) — all I/O
 * goes through spi_transfer/spi_select/spi_deselect.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef FPGC_ENC28J60_H
#define FPGC_ENC28J60_H

/* TODO: Full ENC28J60 API will be ported during Phase 3.
 * See Software/C/libs/kernel/io/enc28j60.h for current API. */

#endif /* FPGC_ENC28J60_H */
