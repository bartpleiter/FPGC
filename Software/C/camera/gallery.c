/*
 * gallery.c — Image gallery viewer
 *
 * Navigates images by number (0..counter-1), computing paths from
 * image numbers via storage_build_path(). Skips gaps (deleted images)
 * automatically. Uses letter keys for navigation:
 *   ,  = previous image
 *   .  = next image
 *   D  = delete current image
 *   G or Escape = exit gallery, return to viewfinder
 */
#include "gallery.h"
#include "bmp.h"
#include "storage.h"
#include "hud.h"
#include "fpgc.h"
#include "viewfinder.h"
#include "gpu_data_ascii.h"
#include "brfs.h"

/* Provided by main.c */
extern int keyboard_poll(void);

/* Current image number being viewed */
static int gallery_current;
/* Total images ever taken (from counter) */
static int gallery_total;

/* Write a short string at a position on the window tile layer */
static void gallery_hud_msg(int col, int row, const char *msg)
{
    unsigned int *tile;
    unsigned int *color;
    int i;

    tile = (unsigned int *)FPGC_GPU_WIN_TILE_TABLE;
    color = (unsigned int *)FPGC_GPU_WIN_COLOR_TABLE;

    for (i = 0; msg[i] != 0 && (col + i) < 40; i++) {
        tile[row * 40 + col + i] = (unsigned int)msg[i];
        color[row * 40 + col + i] = PALETTE_WHITE_ON_BLACK;
    }
}

/* Clear the window tile layer */
static void gallery_clear_hud(void)
{
    unsigned int *tile;
    int i;
    tile = (unsigned int *)FPGC_GPU_WIN_TILE_TABLE;
    for (i = 0; i < 40 * 25; i++) {
        tile[i] = 0;
    }
}

/* Format a number into buf with fixed width, zero-padded */
static void int_to_str(int val, char *buf, int width)
{
    int i;
    for (i = width - 1; i >= 0; i--) {
        buf[i] = '0' + (val % 10);
        val = val / 10;
    }
    buf[width] = 0;
}

/* Show gallery status bar with image number */
static void gallery_update_hud(void)
{
    char status[41];
    char num[6];
    int i;

    for (i = 0; i < 40; i++) status[i] = ' ';
    status[40] = 0;

    if (gallery_total == 0) {
        gallery_clear_hud();
        gallery_hud_msg(10, 12, "No images found");
        gallery_hud_msg(10, 14, "Press G to exit");
        return;
    }

    gallery_clear_hud();

    /* Top line: "[img_NNNNN]" */
    status[0] = '[';
    status[1] = 'i';
    status[2] = 'm';
    status[3] = 'g';
    status[4] = '_';
    int_to_str(gallery_current, num, 5);
    for (i = 0; i < 5; i++) status[5 + i] = num[i];
    status[10] = ']';

    gallery_hud_msg(0, 0, status);

    /* Bottom hint */
    gallery_hud_msg(1, 24, ",=Prev .=Next D=Del G=Exit");
}

/*
 * Find the next existing image at or after 'start', searching forward.
 * Returns image number, or -1 if none found.
 */
static int gallery_find_forward(int start)
{
    char path[40];
    int n;

    for (n = start; n < gallery_total; n++) {
        storage_build_path(n, path, 40);
        if (brfs_exists(&cam_brfs, path)) {
            return n;
        }
    }
    return -1;
}

/*
 * Find the next existing image at or before 'start', searching backward.
 * Returns image number, or -1 if none found.
 */
static int gallery_find_backward(int start)
{
    char path[40];
    int n;

    for (n = start; n >= 0; n--) {
        storage_build_path(n, path, 40);
        if (brfs_exists(&cam_brfs, path)) {
            return n;
        }
    }
    return -1;
}

/* Display image at gallery_current */
static void gallery_show_image(void)
{
    char path[40];

    if (gallery_total == 0) return;

    storage_build_path(gallery_current, path, 40);

    /* Set greyscale palette for image display */
    setup_palette_greyscale();

    bmp_load_to_screen(&cam_brfs, path);
    gallery_update_hud();
}

void gallery_run(void)
{
    int key;
    int found;

    if (!storage_ready) return;

    gallery_total = storage_get_counter();

    if (gallery_total == 0) {
        gallery_clear_hud();
        gallery_hud_msg(10, 12, "No images found");
        gallery_hud_msg(10, 14, "Press M to exit");

        while (1) {
            key = keyboard_poll();
            if (key == 'm' || key == 'M') {
                gallery_clear_hud();
                return;
            }
        }
    }

    /* Start at the most recent image, searching backward for one that exists */
    gallery_current = gallery_find_backward(gallery_total - 1);
    if (gallery_current < 0) {
        /* All images deleted */
        gallery_clear_hud();
        gallery_hud_msg(10, 12, "No images found");
        gallery_hud_msg(10, 14, "Press M to exit");

        while (1) {
            key = keyboard_poll();
            if (key == 'm' || key == 'M') {
                gallery_clear_hud();
                return;
            }
        }
    }

    gallery_show_image();

    /* Gallery input loop */
    while (1) {
        key = keyboard_poll();
        if (key == 0) continue;

        if (key == 'm' || key == 'M') {
            gallery_clear_hud();
            return;
        }

        if (key == 'i' || key == 'I') {
            /* Previous image */
            if (gallery_current > 0) {
                found = gallery_find_backward(gallery_current - 1);
                if (found >= 0) {
                    gallery_current = found;
                    gallery_show_image();
                }
            }
        } else if (key == 'k' || key == 'K') {
            /* Next image */
            if (gallery_current < gallery_total - 1) {
                found = gallery_find_forward(gallery_current + 1);
                if (found >= 0) {
                    gallery_current = found;
                    gallery_show_image();
                }
            }
        } else if (key == ' ') {
            /* Delete current image */
            char dpath[40];
            storage_build_path(gallery_current, dpath, 40);
            brfs_delete(&cam_brfs, dpath);
            storage_sync();

            /* Try to show next image, or previous, or show empty */
            found = gallery_find_forward(gallery_current + 1);
            if (found < 0) {
                found = gallery_find_backward(gallery_current - 1);
            }
            if (found >= 0) {
                gallery_current = found;
                gallery_show_image();
            } else {
                gallery_clear_hud();
                gallery_hud_msg(10, 12, "No images found");
                gallery_hud_msg(10, 14, "Press M to exit");
            }
        }
    }
}
