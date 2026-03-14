#ifndef PLOT_H
#define PLOT_H

// Simple line plotting library for FPGC pixel framebuffer (VRAMPX).
// Uses a built-in 3x5 pixel mini-font for labels.

// Initialize plot region (must be called before other plot functions)
void plot_init(int x, int y, int w, int h);

// Clear plot area with given RRRGGGBB color
void plot_clear(int bg_color);

// Draw axes with tick marks and labels
// y_min, y_max: data range for Y axis
// x_max: maximum X value (for labeling, 0-based)
// axis_color: RRRGGGBB color for axis lines
// grid_color: RRRGGGBB color for grid lines (0 = no grid)
void plot_axes(int y_min, int y_max, int x_max, int axis_color, int grid_color);

// Draw a data series as connected line segments
// data[0..count-1]: Y values
// y_min, y_max: mapping range for Y axis
// color: RRRGGGBB line color
void plot_line(int *data, int count, int y_min, int y_max, int color);

// Draw text at pixel coordinates using 3x5 mini-font
// x, y: top-left pixel position
// str: null-terminated string
// color: RRRGGGBB color
void plot_text(int x, int y, char *str, int color);

// Draw an integer at pixel coordinates using 3x5 mini-font
void plot_number(int x, int y, int val, int color);

#endif // PLOT_H
