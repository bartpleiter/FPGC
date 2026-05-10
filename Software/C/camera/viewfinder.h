/*
 * viewfinder.h - Live viewfinder display via CAM2VRAM DMA
 */
#ifndef VIEWFINDER_H
#define VIEWFINDER_H

/* Display modes */
#define MODE_RAW   0
#define MODE_DITH  1
#define MODE_DITH8 2

/* Palette setup for each display mode */
void setup_palette_4shade(void);
void setup_palette_8shade(void);
void setup_palette_greyscale(void);

/* Load dither threshold/Bayer tables into DMA hardware (call once) */
void load_dither_tables(void);

/* Auto-contrast: compute and upload LUT from hardware min/max stats */
void auto_contrast_from_hw(void);

/* Reset auto-contrast state (call on mode switch) */
void auto_contrast_reset(void);

/* MEM2VRAM blit helpers (for capture preview or fallback) */
void blit_raw_dma(unsigned int src_addr);
void blit_lut_dma(unsigned int src_addr);
void blit_dithered_dma(unsigned int src_addr, int use_lut);
void blit_dithered8_dma(unsigned int src_addr, int use_lut);

/* Run the viewfinder loop (does not return) */
void viewfinder_run(int initial_mode);

#endif
