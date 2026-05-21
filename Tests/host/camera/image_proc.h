/*
 * FPGC-Camera — Image Processing Library
 *
 * Portable C code for the camera image pipeline. Designed to compile
 * on both the B32P3 CPU (via BCC) and x86 host (via gcc) for testing.
 *
 * Pipeline: Y8 input → downsample → auto-contrast → dither → 2-bit output
 */

#ifndef IMAGE_PROC_H
#define IMAGE_PROC_H

/* Use unsigned char for portability between B32P3 (32-bit words) and host */
typedef unsigned char u8;

/*
 * Downsample a greyscale image by 2× in each dimension using 2×2 box averaging.
 *
 * in:    input buffer, in_w × in_h pixels (Y8)
 * out:   output buffer, (in_w/2) × (in_h/2) pixels (Y8)
 * in_w:  input width (must be even)
 * in_h:  input height (must be even)
 */
void downsample_2x2(const u8 *in, u8 *out, int in_w, int in_h);

/*
 * Auto-contrast via histogram stretch.
 *
 * Finds the min and max pixel values in the buffer, then linearly remaps
 * [min, max] → [0, 255]. Operates in-place.
 *
 * buf:   pixel buffer (Y8), modified in place
 * w:     width
 * h:     height
 */
void auto_contrast(u8 *buf, int w, int h);

/*
 * 4×4 ordered dithering. Converts 8-bit greyscale to 2-bit (4 shades).
 *
 * Uses three 4×4 threshold matrices (from the Dashboy Camera project)
 * to produce 4-shade output: 0=black, 1=dark grey, 2=light grey, 3=white.
 *
 * in:    input buffer, w × h pixels (Y8, 0-255)
 * out:   output buffer, w × h pixels (values 0-3)
 * w:     width
 * h:     height
 */
void dither_4x4(const u8 *in, u8 *out, int w, int h);

/*
 * Scale a 2-bit image by 2× in each dimension.
 *
 * Each input pixel becomes a 2×2 block in the output.
 * Input values (0-3) are preserved as palette indices.
 *
 * in:    input buffer, w × h pixels (values 0-3)
 * out:   output buffer, (w*2) × (h*2) pixels (values 0-3)
 * w:     input width
 * h:     input height
 */
void scale_2x(const u8 *in, u8 *out, int w, int h);

#endif /* IMAGE_PROC_H */
