/*
 * menu.h — On-screen settings menu for the camera
 *
 * Renders a settings menu on the GPU window tile layer (rows 1–21)
 * over the live viewfinder. Uses box-drawing characters from the
 * CP437 font for the border.
 */
#ifndef MENU_H
#define MENU_H

/* Open the settings menu */
void menu_open(void);

/* Close the settings menu and restore transparent tiles */
void menu_close(void);

/* Returns 1 if the menu is currently visible */
int menu_is_open(void);

/* Handle a keypress while the menu is open.
 * Returns a pending_action code (0 = none, same codes as viewfinder):
 *   0 = no sensor change needed
 *   2 = mode switch
 *   3 = shutter/ISO/exposure changed
 *   5 = brightness/contrast/orientation/sharpness/gamma changed
 *   6 = resolution change (caller should exit loop)
 */
int menu_handle_key(int key);

/* Redraw the menu (call after settings change) */
void menu_draw(void);

#endif /* MENU_H */
