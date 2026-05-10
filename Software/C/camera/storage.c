/*
 * storage.c — SD card + BRFS filesystem for camera image storage
 *
 * Initializes the SD card, mounts (or formats) a BRFS partition,
 * and provides the DCIM directory for image storage.
 */
#include "storage.h"
#include "sd.h"
#include "brfs.h"
#include "brfs_storage_sdcard.h"
#include "fpgc.h"

/* BRFS state and storage backend */
struct brfs_state cam_brfs;
static brfs_sdcard_storage_t sd_storage;

int storage_ready = 0;
int storage_sd_found = 0;

int storage_init(void)
{
    sd_card_info_t info;
    int rc;

    storage_ready = 0;
    storage_sd_found = 0;

    /* Initialize SD card hardware */
    rc = sd_init(&info);
    if (rc != SD_OK) {
        return -1;
    }
    storage_sd_found = 1;

    /* Initialize storage backend vtable */
    brfs_storage_sdcard_init(&sd_storage);

    /* Initialize BRFS subsystem with cache buffer */
    rc = brfs_init(&cam_brfs, &sd_storage.base,
                   (unsigned int *)CAM_BRFS_CACHE_ADDR,
                   CAM_BRFS_CACHE_WORDS);
    if (rc != BRFS_OK) {
        return -1;
    }

    /* Try to mount existing filesystem */
    rc = brfs_mount(&cam_brfs);
    if (rc == BRFS_OK) {
        storage_ready = 1;
        /* Ensure DCIM directory exists */
        if (!brfs_exists(&cam_brfs, CAM_DCIM_DIR)) {
            brfs_create_dir(&cam_brfs, CAM_DCIM_DIR);
            brfs_sync(&cam_brfs);
        }
        return 0;
    }

    /* Mount failed — SD card present but needs formatting */
    return 1;
}

int storage_format(void)
{
    int rc;

    /* Format with camera parameters */
    rc = brfs_format(&cam_brfs, CAM_BRFS_BLOCKS, CAM_BRFS_WORDS_PER_BLK,
                     CAM_BRFS_LABEL, 1);
    if (rc != BRFS_OK) {
        return -1;
    }

    /* Create DCIM directory */
    rc = brfs_create_dir(&cam_brfs, CAM_DCIM_DIR);
    if (rc != BRFS_OK) {
        return -1;
    }

    /* Sync to SD card */
    rc = brfs_sync(&cam_brfs);
    if (rc != BRFS_OK) {
        return -1;
    }

    storage_ready = 1;
    return 0;
}

int storage_sync(void)
{
    if (!storage_ready) return -1;
    return brfs_sync(&cam_brfs);
}

int storage_next_image_number(void)
{
    struct brfs_dir_entry entries[32];
    int count;
    int max_num;
    int i;
    int num;

    if (!storage_ready) return 1;

    count = brfs_read_dir(&cam_brfs, CAM_DCIM_DIR, entries, 32);
    if (count <= 0) return 1;

    max_num = 0;
    for (i = 0; i < count; i++) {
        char name[17];
        brfs_decompress_string(name, entries[i].filename, 4);
        /* Parse IMG_NNNN.BMP filenames */
        if (name[0] == 'I' && name[1] == 'M' && name[2] == 'G' &&
            name[3] == '_') {
            num = 0;
            num = num * 10 + (name[4] - '0');
            num = num * 10 + (name[5] - '0');
            num = num * 10 + (name[6] - '0');
            num = num * 10 + (name[7] - '0');
            if (num > max_num) max_num = num;
        }
    }

    return max_num + 1;
}

int storage_remaining_images(int res_mode)
{
    unsigned int total_blk;
    unsigned int free_blk;
    unsigned int blk_sz;
    unsigned int blks_per_img;

    if (!storage_ready) return 0;

    brfs_statfs(&cam_brfs, &total_blk, &free_blk, &blk_sz);

    /* BMP file sizes: header (1078) + pixels */
    /* QVGA: 1078 + 76800 = 77878 bytes → ceil(77878/4096) = 20 blocks */
    /* QQVGA: 1078 + 19200 = 20278 bytes → ceil(20278/4096) = 5 blocks */
    if (res_mode == 1) {
        blks_per_img = 5;
    } else {
        blks_per_img = 20;
    }

    return (int)(free_blk / blks_per_img);
}
