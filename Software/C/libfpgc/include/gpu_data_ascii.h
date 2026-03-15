/*
 * gpu_data_ascii.h — Default ASCII font and palette data declarations.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef FPGC_GPU_DATA_ASCII_H
#define FPGC_GPU_DATA_ASCII_H

/* 32 color palettes, each packed as [bg_color:16][fg_color:16] in RRRGGGBB format */
extern const unsigned int gpu_default_palette[32];

/* 256 ASCII character patterns, 4 words per character (8x8 pixels, 2bpp) */
extern const unsigned int gpu_default_patterns[1024];

#endif /* FPGC_GPU_DATA_ASCII_H */
