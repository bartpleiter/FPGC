/*
 * brfs.h — Bart's RAM File System for B32P3/FPGC.
 *
 * Placeholder header. Full port from Software/C/libs/kernel/fs/brfs.c
 * will be done during Phase 3 (BDOS kernel migration).
 *
 * BRFS is pure C (no inline asm, no volatile I/O) — all flash I/O
 * goes through spi_flash functions, all string ops through libc.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef FPGC_BRFS_H
#define FPGC_BRFS_H

/* TODO: Full BRFS API will be ported during Phase 3.
 * See Software/C/libs/kernel/fs/brfs.h for current API. */

#endif /* FPGC_BRFS_H */
