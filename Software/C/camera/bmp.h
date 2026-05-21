/*
 * bmp.h — 8-bit greyscale BMP encoder for camera captures
 */
#ifndef BMP_H
#define BMP_H

#include "brfs.h"

/* BMP header sizes */
#define BMP_FILE_HEADER_SIZE  14
#define BMP_INFO_HEADER_SIZE  40
#define BMP_PALETTE_SIZE      1024   /* 256 entries × 4 bytes (RGBX) */
#define BMP_HEADER_TOTAL      (BMP_FILE_HEADER_SIZE + BMP_INFO_HEADER_SIZE + BMP_PALETTE_SIZE)

/* Image dimensions */
#define BMP_WIDTH  320
#define BMP_HEIGHT 240
#define BMP_PIXELS (BMP_WIDTH * BMP_HEIGHT)  /* 76800 */
#define BMP_FILE_SIZE (BMP_HEADER_TOTAL + BMP_PIXELS) /* 77878 */

/*
 * Save a greyscale frame from SDRAM as a BMP file.
 * src_addr is the byte address of the pixel data in SDRAM.
 * width/height specify the image dimensions.
 * The path should be a full BRFS path, e.g. "/DCIM/IMG_0001.BMP".
 *
 * Returns 0 on success, <0 on error.
 */
int bmp_save(struct brfs_state *fs, const char *path,
             unsigned int src_addr, int width, int height);

/*
 * Load a BMP file from BRFS and display it in VRAMPX.
 * Only supports 8-bit greyscale 320×240 BMPs.
 *
 * Returns 0 on success, <0 on error.
 */
int bmp_load_to_screen(struct brfs_state *fs, const char *path);

#endif /* BMP_H */
