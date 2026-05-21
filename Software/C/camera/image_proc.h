/*
 * image_proc.h — FPGC-Camera image processing pipeline
 *
 * Same algorithms as Tests/host/camera/image_proc.h, adapted for FPGC.
 */
#ifndef IMAGE_PROC_H
#define IMAGE_PROC_H

/* Downsample 320×240 → 160×120 via 2×2 box averaging */
void downsample_2x2(const unsigned char *in, unsigned char *out,
                    int in_w, int in_h);

/* In-place auto-contrast: linear stretch [min,max] → [0,255] */
void auto_contrast(unsigned char *buf, int w, int h);

/* 4×4 ordered dither: 8-bit → 2-bit (values 0-3) */
void dither_4x4(const unsigned char *in, unsigned char *out, int w, int h);

/* 2× nearest-neighbor upscale */
void scale_2x(const unsigned char *in, unsigned char *out, int w, int h);

#endif /* IMAGE_PROC_H */
