/*
 * storage.h — SD card + BRFS filesystem for camera image storage
 */
#ifndef STORAGE_H
#define STORAGE_H

#include "brfs.h"

/* BRFS cache: 24 MiB at 0x2800000 (above stack, below SDRAM end) */
#define CAM_BRFS_CACHE_ADDR   0x2800000
#define CAM_BRFS_CACHE_BYTES  (24 * 1024 * 1024)
#define CAM_BRFS_CACHE_WORDS  (CAM_BRFS_CACHE_BYTES / 4)

/* BRFS format parameters */
#define CAM_BRFS_BLOCKS       6144
#define CAM_BRFS_BYTES_PER_BLK 4096
#define CAM_BRFS_WORDS_PER_BLK (CAM_BRFS_BYTES_PER_BLK / 4)
#define CAM_BRFS_LABEL        "camera"

/* DCIM directory for image storage */
#define CAM_DCIM_DIR          "/DCIM"

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
 * Format the SD card with BRFS and create DCIM directory.
 * Returns 0 on success, <0 on error.
 */
int storage_format(void);

/*
 * Sync BRFS to SD card (flush dirty blocks).
 * Returns 0 on success, <0 on error.
 */
int storage_sync(void);

/*
 * Get the next available image number by scanning DCIM directory.
 * Returns the number (1-based), or 1 if directory is empty/error.
 */
int storage_next_image_number(void);

/*
 * Compute remaining image capacity based on free BRFS blocks.
 * res_mode: 0 = QVGA (320×240), 1 = QQVGA (160×120)
 * Returns number of images that can fit, or 0 if not ready.
 */
int storage_remaining_images(int res_mode);

#endif /* STORAGE_H */
