/*
 * plot.c — Simple line plotting library for FPGC VRAMPX.
 */

#include <plot.h>

/* Current plot region */
static int plot_x;
static int plot_y;
static int plot_w;
static int plot_h;

#define PLOT_MARGIN_LEFT   20
#define PLOT_MARGIN_RIGHT  2
#define PLOT_MARGIN_TOP    8
#define PLOT_MARGIN_BOTTOM 8

/* 3x5 mini-font: packed val = (row0<<12)|(row1<<9)|(row2<<6)|(row3<<3)|row4 */
static int plot_font[59] = {
    0,      /* 32 ' ' */
    0,0,0,0,0,0,0,0,0,0,0,0, /* 33-44 (unused) */
    448,    /* 45 '-' */
    2,      /* 46 '.' */
    4772,   /* 47 '/' */
    31599,  /* 48 '0' */
    11415,  /* 49 '1' */
    29671,  /* 50 '2' */
    29647,  /* 51 '3' */
    23497,  /* 52 '4' */
    31183,  /* 53 '5' */
    31215,  /* 54 '6' */
    29257,  /* 55 '7' */
    31727,  /* 56 '8' */
    31695,  /* 57 '9' */
    1040,   /* 58 ':' */
    0,0,0,0,0,0, /* 59-64 (unused) */
    11245,  /* 65 'A' */
    27566,  /* 66 'B' */
    14627,  /* 67 'C' */
    27502,  /* 68 'D' */
    31207,  /* 69 'E' */
    31140,  /* 70 'F' */
    14699,  /* 71 'G' */
    23533,  /* 72 'H' */
    29847,  /* 73 'I' */
    4714,   /* 74 'J' */
    23469,  /* 75 'K' */
    18727,  /* 76 'L' */
    24557,  /* 77 'M' */
    27501,  /* 78 'N' */
    31599,  /* 79 'O' */
    27556,  /* 80 'P' */
    11089,  /* 81 'Q' */
    27565,  /* 82 'R' */
    14478,  /* 83 'S' */
    29842,  /* 84 'T' */
    23407,  /* 85 'U' */
    23402,  /* 86 'V' */
    23549,  /* 87 'W' */
    23213,  /* 88 'X' */
    23186,  /* 89 'Y' */
    29351   /* 90 'Z' */
};

/* ---- Pixel helpers ---- */

void plot_put_pixel(int x, int y, int color)
{
    if (x < 0 || x >= PIXEL_FB_WIDTH || y < 0 || y >= PIXEL_FB_HEIGHT)
        return;
    /* VRAMPX is byte-addressable: one byte per pixel. */
    __builtin_storeb(PIXEL_FB_ADDR + (y * PIXEL_FB_WIDTH + x), color);
}

void plot_hline(int x0, int x1, int y, int color)
{
    int x;
    for (x = x0; x <= x1; x++)
        plot_put_pixel(x, y, color);
}

void plot_vline(int x, int y0, int y1, int color)
{
    int y;
    for (y = y0; y <= y1; y++)
        plot_put_pixel(x, y, color);
}

void plot_draw_line(int x0, int y0, int x1, int y1, int color)
{
    int dx, dy, sx, sy, err, e2;

    dx = (x0 < x1) ? x1 - x0 : x0 - x1;
    sx = (x0 < x1) ? 1 : -1;
    dy = (y0 < y1) ? y1 - y0 : y0 - y1;
    sy = (y0 < y1) ? 1 : -1;
    err = (dx > dy) ? dx / 2 : -(dy / 2);

    for (;;)
    {
        plot_put_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1)
            break;
        e2 = err;
        if (e2 > -dx)
        {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dy)
        {
            err += dx;
            y0 += sy;
        }
    }
}

/* ---- Font rendering ---- */

void plot_draw_char(int px, int py, int ch, int color)
{
    int font_idx, packed, row, col, row_bits;

    if (ch >= 97 && ch <= 122)
        ch -= 32;

    font_idx = ch - 32;
    if (font_idx < 0 || font_idx > 58)
        return;

    packed = plot_font[font_idx];
    if (packed == 0 && ch != 32)
        return;

    for (row = 0; row < 5; row++)
    {
        row_bits = (packed >> (12 - row * 3)) & 7;
        for (col = 0; col < 3; col++)
        {
            if (row_bits & (4 >> col))
                plot_put_pixel(px + col, py + row, color);
        }
    }
}

void plot_text(int x, int y, char *str, int color)
{
    int i;
    for (i = 0; str[i] != 0; i++)
    {
        plot_draw_char(x, y, str[i], color);
        x += 4;
    }
}

void plot_number(int x, int y, int val, int color)
{
    char buf[12];
    int i, neg;
    unsigned int uval;

    if (val == 0)
    {
        plot_draw_char(x, y, '0', color);
        return;
    }

    neg = 0;
    if (val < 0)
    {
        neg = 1;
        uval = (unsigned int)(-val);
    }
    else
    {
        uval = (unsigned int)val;
    }

    i = 11;
    buf[i] = 0;
    while (uval > 0)
    {
        i--;
        buf[i] = '0' + (uval % 10);
        uval = uval / 10;
    }
    if (neg)
    {
        i--;
        buf[i] = '-';
    }
    plot_text(x, y, buf + i, color);
}

/* ---- Plot functions ---- */

void plot_init(int x, int y, int w, int h)
{
    plot_x = x;
    plot_y = y;
    plot_w = w;
    plot_h = h;
}

void plot_clear(int bg_color)
{
    int px, py;
    for (py = plot_y; py < plot_y + plot_h; py++)
    {
        if (py >= 0 && py < PIXEL_FB_HEIGHT)
        {
            for (px = plot_x; px < plot_x + plot_w; px++)
            {
                if (px >= 0 && px < PIXEL_FB_WIDTH)
                    __builtin_storeb(PIXEL_FB_ADDR + (py * PIXEL_FB_WIDTH + px), bg_color);
            }
        }
    }
}

void plot_axes(int y_min, int y_max, int x_max, int axis_color, int grid_color)
{
    int data_x0, data_y0, data_x1, data_y1;
    int data_w, data_h;
    int nticks, i, step, tick_val, tick_y;

    data_x0 = plot_x + PLOT_MARGIN_LEFT;
    data_y0 = plot_y + PLOT_MARGIN_TOP;
    data_x1 = plot_x + plot_w - 1 - PLOT_MARGIN_RIGHT;
    data_y1 = plot_y + plot_h - 1 - PLOT_MARGIN_BOTTOM;
    data_w = data_x1 - data_x0;
    data_h = data_y1 - data_y0;

    plot_vline(data_x0, data_y0, data_y1, axis_color);
    plot_hline(data_x0, data_x1, data_y1, axis_color);

    nticks = 4;
    if (y_max <= y_min)
        return;

    step = (y_max - y_min) / nticks;
    if (step < 1)
        step = 1;

    for (i = 0; i <= nticks; i++)
    {
        tick_val = y_min + i * step;
        if (tick_val > y_max)
            tick_val = y_max;

        tick_y = data_y1 - ((tick_val - y_min) * data_h) / (y_max - y_min);

        plot_hline(data_x0 - 2, data_x0, tick_y, axis_color);

        if (grid_color != 0 && i > 0 && i < nticks)
        {
            int gx;
            for (gx = data_x0 + 2; gx <= data_x1; gx += 4)
                plot_put_pixel(gx, tick_y, grid_color);
        }

        plot_number(plot_x + 1, tick_y - 2, tick_val, axis_color);
    }
}

static int plot_line_map_y(int val, int y_min, int y_max, int data_y1, int data_h)
{
    int range;
    range = y_max - y_min;
    if (val < y_min) val = y_min;
    if (val > y_max) val = y_max;
    return data_y1 - ((val - y_min) * data_h) / range;
}

void plot_line(int *data, int count, int y_min, int y_max, int color)
{
    int data_x0, data_x1, data_y1;
    int data_w, data_h;
    int i, px0, py0, px1, py1;

    if (count < 2 || y_max <= y_min)
        return;

    data_x0 = plot_x + PLOT_MARGIN_LEFT;
    data_x1 = plot_x + plot_w - 1 - PLOT_MARGIN_RIGHT;
    data_y1 = plot_y + plot_h - 1 - PLOT_MARGIN_BOTTOM;
    data_w = data_x1 - data_x0;
    data_h = data_y1 - (plot_y + PLOT_MARGIN_TOP);

    px0 = data_x0;
    py0 = plot_line_map_y(data[0], y_min, y_max, data_y1, data_h);

    for (i = 1; i < count; i++)
    {
        px1 = data_x0 + (i * data_w) / (count - 1);
        py1 = plot_line_map_y(data[i], y_min, y_max, data_y1, data_h);

        plot_draw_line(px0, py0, px1, py1, color);

        px0 = px1;
        py0 = py1;
    }
}
