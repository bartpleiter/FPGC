#ifndef FPGC_GPU_DATA_ASCII_H
#define FPGC_GPU_DATA_ASCII_H

/* Palette index constants */
#define PALETTE_WHITE_ON_BLACK       0
#define PALETTE_BLACK_ON_WHITE       1
#define PALETTE_RED_ON_BLACK         2
#define PALETTE_GREEN_ON_BLACK       3
#define PALETTE_BLUE_ON_BLACK        4
#define PALETTE_YELLOW_ON_BLACK      5
#define PALETTE_MAGENTA_ON_BLACK     6
#define PALETTE_CYAN_ON_BLACK        7
#define PALETTE_RED_ON_WHITE         8
#define PALETTE_GREEN_ON_WHITE       9
#define PALETTE_BLUE_ON_WHITE       10
#define PALETTE_GRAY_ON_BLACK       11
#define PALETTE_GRAY_ON_WHITE       12
#define PALETTE_ORANGE_ON_BLACK     13
#define PALETTE_DARKGRAY_ON_BLACK   14
#define PALETTE_LIGHTGRAY_ON_BLACK  15
#define PALETTE_ORANGE_ON_WHITE     16
#define PALETTE_DARKGRAY_ON_WHITE   17
#define PALETTE_BROWN_ON_BLACK      18
#define PALETTE_LIGHTYELLOW_ON_BLACK 19
#define PALETTE_LIGHTGREEN_ON_BLACK 20
#define PALETTE_LIGHTRED_ON_BLACK   21
#define PALETTE_LIGHTBLUE_ON_BLACK  22
#define PALETTE_LIGHTYELLOW_ON_WHITE 23
#define PALETTE_LIGHTGREEN_ON_WHITE 24
#define PALETTE_LIGHTRED_ON_WHITE   25
#define PALETTE_YELLOW_ON_WHITE     26
#define PALETTE_CYAN_ON_WHITE       27
#define PALETTE_BLACK_ON_YELLOW     28
#define PALETTE_GREEN_ON_YELLOW     30
#define PALETTE_WHITE_ON_RED        31

/* 32 color palettes, each packed as [bg_color:16][fg_color:16] in RRRGGGBB format */
extern const unsigned int gpu_default_palette[32];

/* 256 ASCII character patterns, 4 words per character (8x8 pixels, 2bpp) */
extern const unsigned int gpu_default_patterns[1024];

#endif /* FPGC_GPU_DATA_ASCII_H */
