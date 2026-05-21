/*
 * FPGC-Camera — PGM Image I/O Utilities
 *
 * Simple PGM (P5 binary) reader/writer for test images.
 * PGM is a trivially simple greyscale image format, perfect for testing.
 */

#ifndef PGM_IO_H
#define PGM_IO_H

typedef unsigned char u8;

/*
 * Read a PGM (P5) image from file.
 * Returns pixel buffer (caller must free), sets *w and *h.
 * Returns NULL on error.
 */
u8 *pgm_read(const char *path, int *w, int *h);

/*
 * Write a PGM (P5) image to file.
 * Returns 0 on success, -1 on error.
 */
int pgm_write(const char *path, const u8 *buf, int w, int h);

#endif /* PGM_IO_H */
