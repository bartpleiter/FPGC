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

/* Update the full HUD display (call after settings change or periodically) */
void hud_update(int fps);

/* Clear the HUD (hide all text) */
void hud_clear(void);

#endif /* HUD_H */
