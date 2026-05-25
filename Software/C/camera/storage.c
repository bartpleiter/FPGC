/*
 * storage.c — SD card + BRFS filesystem for camera image storage
 *
 * Initializes the SD card, mounts (or formats) a 1 GiB BRFS partition
 * with LRU caching, and manages image files across numbered
 * subdirectories (dir_0, dir_1, ...) with a persistent counter file.
 */
#include "storage.h"
#include "sd.h"
#include "brfs.h"
#include "brfs_storage_sdcard.h"
#include "fpgc.h"
#include <string.h>

/* BRFS state and storage backend */
struct brfs_state cam_brfs;
static brfs_sdcard_storage_t sd_storage;

int storage_ready = 0;
int storage_sd_found = 0;

/* Persistent image counter (loaded from /counter on mount) */
static int image_counter = 0;

/* ---- Counter file helpers ---- */

static int storage_load_counter(void)
{
    int fd;
    char buf[12];
    int rc;
    int val;
    int i;

    fd = brfs_open(&cam_brfs, CAM_COUNTER_FILE);
    if (fd < 0) {
        image_counter = 0;
        return -1;
    }

    rc = brfs_read(&cam_brfs, fd, buf, 11);
    brfs_close(&cam_brfs, fd);
    if (rc <= 0) {
        image_counter = 0;
        return -1;
    }
    buf[rc] = 0;

    /* Parse ASCII decimal */
    val = 0;
    for (i = 0; buf[i] >= '0' && buf[i] <= '9'; i++) {
        val = val * 10 + (buf[i] - '0');
    }
    image_counter = val;
    return 0;
}

static int storage_save_counter(void)
{
    int fd;
    char buf[12];
    int val;
    int len;
    int i;
    int rc;

    /* Convert counter to ASCII decimal */
    val = image_counter;
    if (val == 0) {
        buf[0] = '0';
        len = 1;
    } else {
        len = 0;
        while (val > 0) {
            buf[len++] = '0' + (val % 10);
            val = val / 10;
        }
        /* Reverse */
        for (i = 0; i < len / 2; i++) {
            char tmp;
            tmp = buf[i];
            buf[i] = buf[len - 1 - i];
            buf[len - 1 - i] = tmp;
        }
    }
    buf[len] = 0;

    /* Overwrite counter file (open, truncate, write) */
    fd = brfs_open(&cam_brfs, CAM_COUNTER_FILE);
    if (fd < 0) {
        /* File doesn't exist yet — create it */
        rc = brfs_create_file(&cam_brfs, CAM_COUNTER_FILE);
        if (rc != BRFS_OK) return -1;
        fd = brfs_open(&cam_brfs, CAM_COUNTER_FILE);
        if (fd < 0) return -1;
    }
    brfs_truncate(&cam_brfs, fd);
    brfs_seek(&cam_brfs, fd, 0);
    rc = brfs_write(&cam_brfs, fd, buf, (unsigned int)len);
    brfs_close(&cam_brfs, fd);
    return (rc >= 0) ? 0 : -1;
}

/* ---- Directory helpers ---- */

/*
 * Build the directory name for image number N.
 * dir_index = N / CAM_FILES_PER_DIR
 * Result: "/dir_Y" (null-terminated)
 */
static void build_dir_path(int image_num, char *buf)
{
    int dir_idx;
    int i;
    int d;
    int started;

    dir_idx = image_num / CAM_FILES_PER_DIR;

    buf[0] = '/';
    buf[1] = 'd';
    buf[2] = 'i';
    buf[3] = 'r';
    buf[4] = '_';

    /* Write dir index as decimal */
    i = 5;
    if (dir_idx == 0) {
        buf[i++] = '0';
    } else {
        /* Find highest digit place (max ~125 dirs) */
        started = 0;
        for (d = 1000; d > 0; d = d / 10) {
            int digit;
            digit = (dir_idx / d) % 10;
            if (digit != 0 || started) {
                buf[i++] = '0' + digit;
                started = 1;
            }
        }
    }
    buf[i] = 0;
}

static int storage_ensure_dir(int image_num)
{
    char dir_path[16];

    build_dir_path(image_num, dir_path);

    if (brfs_exists(&cam_brfs, dir_path)) {
        return 0;
    }

    return brfs_create_dir(&cam_brfs, dir_path);
}

/* ---- Public API ---- */

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

    /* Override on-disk layout for 1 GiB partition (FAT needs 1 MiB) */
    brfs_cache_set_layout(&cam_brfs.cache_state,
                          CAM_BRFS_SB_ADDR,
                          CAM_BRFS_FAT_ADDR,
                          CAM_BRFS_DATA_ADDR);

    /* Try to mount existing filesystem */
    rc = brfs_mount(&cam_brfs);
    if (rc == BRFS_OK) {
        storage_ready = 1;
        storage_load_counter();
        return 0;
    }

    /* Mount failed — SD card present but needs formatting */
    return 1;
}

int storage_format(void)
{
    int rc;

    /* Format: 1 GiB, 4 KiB blocks, no full format (fast) */
    rc = brfs_format(&cam_brfs, CAM_BRFS_BLOCKS, CAM_BRFS_WORDS_PER_BLK,
                     CAM_BRFS_LABEL, 0);
    if (rc != BRFS_OK) {
        return -1;
    }

    /* Create counter file initialised to "0" */
    image_counter = 0;
    rc = storage_save_counter();
    if (rc != 0) {
        return -1;
    }

    /* Sync to SD card (fast: only FAT + root block + counter data) */
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

int storage_next_image(char *path_out, int path_size)
{
    char dir_path[16];
    int num;
    int rc;
    int i;
    int d;
    int started;
    int pos;

    if (!storage_ready) return -1;

    num = image_counter;

    /* Ensure target directory exists */
    rc = storage_ensure_dir(num);
    if (rc != BRFS_OK && rc != 0) return -1;

    /* Build full path: /dir_Y/img_X.bmp */
    build_dir_path(num, dir_path);

    /* Copy dir path */
    pos = 0;
    for (i = 0; dir_path[i] != 0 && pos < path_size - 1; i++) {
        path_out[pos++] = dir_path[i];
    }

    /* Separator */
    if (pos < path_size - 1) path_out[pos++] = '/';

    /* "img_" */
    if (pos < path_size - 4) {
        path_out[pos++] = 'i';
        path_out[pos++] = 'm';
        path_out[pos++] = 'g';
        path_out[pos++] = '_';
    }

    /* Image number as decimal */
    if (num == 0) {
        if (pos < path_size - 1) path_out[pos++] = '0';
    } else {
        started = 0;
        for (d = 100000; d > 0; d = d / 10) {
            int digit;
            digit = (num / d) % 10;
            if (digit != 0 || started) {
                if (pos < path_size - 1) path_out[pos++] = '0' + digit;
                started = 1;
            }
        }
    }

    /* ".bmp" */
    if (pos < path_size - 4) {
        path_out[pos++] = '.';
        path_out[pos++] = 'b';
        path_out[pos++] = 'm';
        path_out[pos++] = 'p';
    }
    path_out[pos] = 0;

    /* Advance counter and persist */
    image_counter = num + 1;
    storage_save_counter();

    return num;
}

void storage_build_path(int image_num, char *path_out, int path_size)
{
    char dir_path[16];
    int i;
    int d;
    int started;
    int pos;

    build_dir_path(image_num, dir_path);

    pos = 0;
    for (i = 0; dir_path[i] != 0 && pos < path_size - 1; i++) {
        path_out[pos++] = dir_path[i];
    }
    if (pos < path_size - 1) path_out[pos++] = '/';
    if (pos < path_size - 4) {
        path_out[pos++] = 'i';
        path_out[pos++] = 'm';
        path_out[pos++] = 'g';
        path_out[pos++] = '_';
    }
    if (image_num == 0) {
        if (pos < path_size - 1) path_out[pos++] = '0';
    } else {
        started = 0;
        for (d = 100000; d > 0; d = d / 10) {
            int digit;
            digit = (image_num / d) % 10;
            if (digit != 0 || started) {
                if (pos < path_size - 1) path_out[pos++] = '0' + digit;
                started = 1;
            }
        }
    }
    if (pos < path_size - 4) {
        path_out[pos++] = '.';
        path_out[pos++] = 'b';
        path_out[pos++] = 'm';
        path_out[pos++] = 'p';
    }
    path_out[pos] = 0;
}

int storage_get_counter(void)
{
    return image_counter;
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
