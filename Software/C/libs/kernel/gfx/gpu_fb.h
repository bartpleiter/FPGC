#ifndef GPU_FB_H
#define GPU_FB_H

/*
 * GPU Framebuffer Library
 * Provides functions to manipulate the pixel plane of the GPU.
 */

#define FB_WIDTH 320
#define FB_HEIGHT 240

void fb_clear(void);
void fb_set_pixel(unsigned int x, unsigned int y, unsigned int color);
void fb_draw_line(unsigned int x0, unsigned int y0, unsigned int x1, unsigned int y1, unsigned int color);
void fb_draw_rect(unsigned int x, unsigned int y, unsigned int w, unsigned int h, unsigned int color);
void fb_fill_rect(unsigned int x, unsigned int y, unsigned int w, unsigned int h, unsigned int color);
void fb_draw_circle(unsigned int x, unsigned int y, unsigned int radius, unsigned int color);
void fb_blit(unsigned int x, unsigned int y, unsigned int width, unsigned int height, const unsigned int *data);

#endif // GPU_FB_H
