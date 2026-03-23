#ifndef PLOT_H
#define PLOT_H

/*
 * Simple line plotting library for FPGC pixel framebuffer (VRAMPX).
 * Uses a built-in 3x5 pixel mini-font for labels.
 */

/* GPU pixel framebuffer */
#define PIXEL_FB_ADDR    0x1EC00000
#define PIXEL_FB_WIDTH   320
#define PIXEL_FB_HEIGHT  240

/* GPU pixel palette */
#define PIXEL_PALETTE_ADDR 0x1EC80000

void plot_init(int x, int y, int w, int h);
void plot_clear(int bg_color);
void plot_axes(int y_min, int y_max, int x_max, int axis_color, int grid_color);
void plot_line(int *data, int count, int y_min, int y_max, int color);
void plot_text(int x, int y, char *str, int color);
void plot_number(int x, int y, int val, int color);

/* Low-level drawing primitives (also usable directly) */
void plot_put_pixel(int x, int y, int color);
void plot_hline(int x0, int x1, int y, int color);
void plot_vline(int x, int y0, int y1, int color);
void plot_draw_line(int x0, int y0, int x1, int y1, int color);
void plot_draw_char(int px, int py, int ch, int color);

#endif /* PLOT_H */
