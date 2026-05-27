/*
 * hud.h — Camera HUD overlay on GPU window layer
 *
 * Renders shooting mode, shutter speed, ISO, EV comp, and FPS
 * info on the 40×25 text window layer (overlays the pixel buffer).
 */
#ifndef HUD_H
#define HUD_H

/* Initialize the HUD (load patterns/palettes, clear window) */
void hud_init(void);

/* Update the full HUD display.
 * qa_active: current quick-adjust parameter index (QA_BRIGHTNESS..QA_GAMMA)
 * qa_highlight: highlight timer (>0 = show bracket markers on active param) */
void hud_update(int fps, int qa_active, int qa_highlight);

/* Clear the HUD (hide all text) */
void hud_clear(void);

/* Show splash screen (centered title on row 12) */
void hud_splash(const char *title);

#endif /* HUD_H */
