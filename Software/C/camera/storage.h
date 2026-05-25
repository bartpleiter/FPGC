/*
 * storage.h — SD card + BRFS filesystem for camera image storage
 */
#ifndef STORAGE_H
#define STORAGE_H

#include "brfs.h"

/* BRFS cache: 4 MiB at 0x2800000 (matches BDOS SD cache size) */
#define CAM_BRFS_CACHE_ADDR   0x2800000
#define CAM_BRFS_CACHE_BYTES  (4 * 1024 * 1024)
#define CAM_BRFS_CACHE_WORDS  (CAM_BRFS_CACHE_BYTES / 4)

/* BRFS format parameters — 1 GiB partition, 4 KiB blocks */
#define CAM_BRFS_BLOCKS        262144
#define CAM_BRFS_BYTES_PER_BLK 4096
#define CAM_BRFS_WORDS_PER_BLK (CAM_BRFS_BYTES_PER_BLK / 4)
#define CAM_BRFS_LABEL         "camera"

/* On-disk layout: FAT needs 1 MiB for 262144 entries */
#define CAM_BRFS_SB_ADDR       0x000000
#define CAM_BRFS_FAT_ADDR      0x001000
#define CAM_BRFS_DATA_ADDR     0x101000

/* Directory organisation: 100 images per directory */
#define CAM_FILES_PER_DIR      100
#define CAM_COUNTER_FILE       "/counter"

/* Global BRFS state */
extern struct brfs_state cam_brfs;

/* Storage status */
extern int storage_ready;     /* 1 if BRFS mounted and ready */
extern int storage_sd_found;  /* 1 if SD card detected */

/*
 * Initialize SD card and attempt to mount BRFS.
 * Returns:
 *   0 = mounted OK, ready for use
 *   1 = SD card found but no BRFS (needs format)
 *  -1 = no SD card
 */
int storage_init(void);

/*
 * Format the SD card with BRFS (1 GiB, no full format).
 * Creates counter file initialised to 0.
 * Returns 0 on success, <0 on error.
 */
int storage_format(void);

/*
 * Sync BRFS to SD card (flush dirty blocks).
 * Returns 0 on success, <0 on error.
 */
int storage_sync(void);

/*
 * Get the next image number, build path, advance counter.
 * Creates the target directory if it doesn't exist.
 * Writes the path (e.g. "/dir_2/img_250.bmp") into path_out.
 * Returns image number on success, or -1 on error.
 */
int storage_next_image(char *path_out, int path_size);

/*
 * Build the path for an existing image number (without advancing counter).
 * Writes e.g. "/dir_2/img_250.bmp" into path_out.
 */
void storage_build_path(int image_num, char *path_out, int path_size);

/*
 * Get the current image counter (next image number to be assigned).
 */
int storage_get_counter(void);

/*
 * Compute remaining image capacity based on free BRFS blocks.
 * res_mode: 0 = QVGA (320×240), 1 = QQVGA (160×120)
 * Returns number of images that can fit, or 0 if not ready.
 */
int storage_remaining_images(int res_mode);

#endif /* STORAGE_H */
