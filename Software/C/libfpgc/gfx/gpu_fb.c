#include "gpu_hal.h"
#include "gpu_fb.h"

void
fb_clear(void)
{
    gpu_clear_pixel();
}

void
fb_set_pixel(unsigned int x, unsigned int y, unsigned int color)
{
    gpu_write_pixel_data(x, y, color);
}

void
fb_draw_line(int x0, int y0, int x1, int y1, unsigned int color)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1;
    int dy = y1 > y0 ? y1 - y0 : y0 - y1;
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;
    int e2;

    for (;;) {
        gpu_write_pixel_data((unsigned int)x0, (unsigned int)y0, color);
        if (x0 == x1 && y0 == y1)
            break;
        e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void
fb_draw_rect(int x, int y, int w, int h, unsigned int color)
{
    if (w == 0 || h == 0)
        return;
    fb_draw_line(x, y, x + w - 1, y, color);
    fb_draw_line(x, y + h - 1, x + w - 1, y + h - 1, color);
    fb_draw_line(x, y, x, y + h - 1, color);
    fb_draw_line(x + w - 1, y, x + w - 1, y + h - 1, color);
}

void
fb_fill_rect(unsigned int x, unsigned int y, unsigned int w, unsigned int h, unsigned int color)
{
    unsigned int i, j;
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++)
            gpu_write_pixel_data(x + i, y + j, color);
}

void
fb_draw_circle(int x, int y, int radius, unsigned int color)
{
    int xp = radius;
    int yp = 0;
    int err = 0;

    while (xp >= yp) {
        gpu_write_pixel_data((unsigned int)(x + xp), (unsigned int)(y + yp), color);
        gpu_write_pixel_data((unsigned int)(x + yp), (unsigned int)(y + xp), color);
        gpu_write_pixel_data((unsigned int)(x - yp), (unsigned int)(y + xp), color);
        gpu_write_pixel_data((unsigned int)(x - xp), (unsigned int)(y + yp), color);
        gpu_write_pixel_data((unsigned int)(x - xp), (unsigned int)(y - yp), color);
        gpu_write_pixel_data((unsigned int)(x - yp), (unsigned int)(y - xp), color);
        gpu_write_pixel_data((unsigned int)(x + yp), (unsigned int)(y - xp), color);
        gpu_write_pixel_data((unsigned int)(x + xp), (unsigned int)(y - yp), color);

        if (err <= 0) {
            yp++;
            err += 2 * yp + 1;
        }
        if (err > 0) {
            xp--;
            err -= 2 * xp + 1;
        }
    }
}

void
fb_blit(unsigned int x, unsigned int y, unsigned int width, unsigned int height,
        const unsigned int *data)
{
    unsigned int i, j;
    for (j = 0; j < height; j++)
        for (i = 0; i < width; i++)
            gpu_write_pixel_data(x + i, y + j, data[j * width + i]);
}
