/*
 * debug.h — Memory debug utilities for B32P3/FPGC.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef FPGC_DEBUG_H
#define FPGC_DEBUG_H

/* Hex-dump a memory range over UART */
void debug_mem_dump(unsigned int *start, unsigned int length);

#endif /* FPGC_DEBUG_H */
