/*
 * gallery.c — Image gallery viewer
 *
 * Lists BMP files in /DCIM, displays them one at a time, and allows
 * browsing with keyboard. Arrow keys are not available (HID table
 * maps them to 0), so we use letter keys:
 *   ,  = previous image
 *   .  = next image
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

/* Max images we can index */
#define GALLERY_MAX 32

static struct brfs_dir_entry gallery_entries[GALLERY_MAX];
static int gallery_count;
static int gallery_index;

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

/* Format a number into buf, return pointer to start */
static void int_to_str(int val, char *buf, int width)
{
    int i;
    for (i = width - 1; i >= 0; i--) {
        buf[i] = '0' + (val % 10);
        val = val / 10;
    }
    buf[width] = 0;
}

/* Show gallery status bar: "IMG 3/12  IMG_0003.BMP" */
static void gallery_update_hud(void)
{
    char status[41];
    char num[5];
    int i;

    /* Clear status line */
    for (i = 0; i < 40; i++) status[i] = ' ';
    status[40] = 0;

    if (gallery_count == 0) {
        gallery_clear_hud();
        gallery_hud_msg(10, 12, "No images found");
        gallery_hud_msg(10, 14, "Press G to exit");
        return;
    }

    gallery_clear_hud();

    /* Top line: "IMG n/total" */
    status[0] = '[';
    int_to_str(gallery_index + 1, num, 4);
    for (i = 0; i < 4; i++) status[1 + i] = num[i];
    status[5] = '/';
    int_to_str(gallery_count, num, 4);
    for (i = 0; i < 4; i++) status[6 + i] = num[i];
    status[10] = ']';
    status[11] = ' ';

    /* Filename */
    {
        char fname[17];
        brfs_decompress_string(fname, gallery_entries[gallery_index].filename, 4);
        for (i = 0; i < 16 && fname[i] != 0; i++) {
            status[12 + i] = fname[i];
        }
    }

    gallery_hud_msg(0, 0, status);

    /* Bottom hint */
    gallery_hud_msg(1, 24, ",=Prev .=Next D=Del G=Exit");
}

/* Display image at gallery_index */
static void gallery_show_image(void)
{
    char path[32];
    char fname[17];
    int i;

    if (gallery_count == 0) return;

    /* Build path: /DCIM/<filename> */
    path[0] = '/';
    path[1] = 'D';
    path[2] = 'C';
    path[3] = 'I';
    path[4] = 'M';
    path[5] = '/';
    brfs_decompress_string(fname, gallery_entries[gallery_index].filename, 4);
    for (i = 0; i < 16 && fname[i] != 0; i++) {
        path[6 + i] = fname[i];
    }
    path[6 + i] = 0;

    /* Set greyscale palette for image display */
    setup_palette_greyscale();

    bmp_load_to_screen(&cam_brfs, path);
    gallery_update_hud();
}

void gallery_run(void)
{
    int key;
    int i;
    int count;

    if (!storage_ready) return;

    /* Scan DCIM directory for BMP files */
    count = brfs_read_dir(&cam_brfs, CAM_DCIM_DIR,
                          gallery_entries, GALLERY_MAX);
    gallery_count = 0;
    if (count > 0) {
        /* Filter: keep only non-directory entries (BMP files) */
        for (i = 0; i < count; i++) {
            if (!(gallery_entries[i].flags & BRFS_FLAG_DIRECTORY)) {
                if (gallery_count != i) {
                    gallery_entries[gallery_count] = gallery_entries[i];
                }
                gallery_count++;
            }
        }
    }

    gallery_index = 0;

    /* Show first image or "empty" message */
    if (gallery_count > 0) {
        gallery_show_image();
    } else {
        gallery_update_hud();
    }

    /* Gallery input loop */
    while (1) {
        key = keyboard_poll();
        if (key == 0) continue;

        if (key == 'g' || key == 'G' || key == 27) {
            /* Exit gallery */
            gallery_clear_hud();
            return;
        }

        if (gallery_count == 0) continue;

        if (key == ',' || key == '<') {
            /* Previous image */
            if (gallery_index > 0) {
                gallery_index--;
                gallery_show_image();
            }
        } else if (key == '.' || key == '>') {
            /* Next image */
            if (gallery_index < gallery_count - 1) {
                gallery_index++;
                gallery_show_image();
            }
        } else if (key == 'd' || key == 'D') {
            /* Delete current image */
            char dpath[32];
            char dfname[17];
            int di;
            dpath[0] = '/';
            dpath[1] = 'D';
            dpath[2] = 'C';
            dpath[3] = 'I';
            dpath[4] = 'M';
            dpath[5] = '/';
            brfs_decompress_string(dfname,
                gallery_entries[gallery_index].filename, 4);
            for (di = 0; di < 16 && dfname[di] != 0; di++) {
                dpath[6 + di] = dfname[di];
            }
            dpath[6 + di] = 0;
            brfs_delete(&cam_brfs, dpath);
            storage_sync();
            /* Re-scan directory */
            {
                int new_count;
                int ni;
                new_count = brfs_read_dir(&cam_brfs, CAM_DCIM_DIR,
                                          gallery_entries, GALLERY_MAX);
                gallery_count = 0;
                if (new_count > 0) {
                    for (ni = 0; ni < new_count; ni++) {
                        if (!(gallery_entries[ni].flags & BRFS_FLAG_DIRECTORY)) {
                            if (gallery_count != ni) {
                                gallery_entries[gallery_count] = gallery_entries[ni];
                            }
                            gallery_count++;
                        }
                    }
                }
            }
            if (gallery_index >= gallery_count) {
                gallery_index = gallery_count - 1;
            }
            if (gallery_index < 0) gallery_index = 0;
            if (gallery_count > 0) {
                gallery_show_image();
            } else {
                gallery_update_hud();
            }
        }
    }
}
