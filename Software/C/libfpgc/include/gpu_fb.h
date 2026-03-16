#ifndef FPGC_GPU_FB_H
#define FPGC_GPU_FB_H

void fb_clear(void);
void fb_set_pixel(unsigned int x, unsigned int y, unsigned int color);
void fb_draw_line(int x0, int y0, int x1, int y1, unsigned int color);
void fb_draw_rect(int x, int y, int w, int h, unsigned int color);
void fb_fill_rect(unsigned int x, unsigned int y, unsigned int w, unsigned int h, unsigned int color);
void fb_draw_circle(int x, int y, int radius, unsigned int color);
void fb_blit(unsigned int x, unsigned int y, unsigned int width, unsigned int height, const unsigned int *data);

#endif /* FPGC_GPU_FB_H */
