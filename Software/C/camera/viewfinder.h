/*
 * viewfinder.h - Live viewfinder display via CAM2VRAM DMA
 */
#ifndef VIEWFINDER_H
#define VIEWFINDER_H

/* Display modes */
#define MODE_RAW   0
#define MODE_DITH  1
#define MODE_DITH8 2

/* Resolution modes */
#define RES_QVGA   0   /* 320×240 native */
#define RES_QQVGA  1   /* 160×120, 2× upscaled */

/* QQVGA dimensions */
#define QQVGA_W     160
#define QQVGA_H     120
#define QQVGA_BYTES (QQVGA_W * QQVGA_H)  /* 19200 */

/* Palette setup for each display mode */
void setup_palette_4shade(void);
void setup_palette_8shade(void);
void setup_palette_greyscale(void);

/* Load dither threshold/Bayer tables into DMA hardware (call once) */
void load_dither_tables(void);

/* MEM2VRAM blit helpers (for capture preview or fallback) */
void blit_raw_dma(unsigned int src_addr);
void blit_dithered_dma(unsigned int src_addr);
void blit_dithered8_dma(unsigned int src_addr);

/* Physical button mapping (right-hand 8-key layout) */
#define BTN_UP      'i'
#define BTN_DOWN    'k'
#define BTN_LEFT    'j'
#define BTN_RIGHT   'l'
#define BTN_SHUTTER ' '
#define BTN_MENU    'm'
#define BTN_FN1     'u'
#define BTN_FN2     'o'

/* Run the viewfinder loop (does not return) */
void viewfinder_run(int initial_mode);

#endif
