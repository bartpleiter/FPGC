/*
 * fs.c — Filesystem layer: BRFS mount + file_ops for VFS integration.
 *
 * Mounts SPI flash (always) and SD card (if present).
 * Provides file_ops for BRFS-backed files.
 */
#include "kernel.h"
#include "brfs_storage_spi_flash.h"
#include "brfs_storage_sdcard.h"

/* BRFS state for SPI flash and SD card */
struct brfs_state brfs_spi;
struct brfs_state brfs_sd;
int fs_sd_ready;

/* Storage backends */
static brfs_spi_flash_storage_t fs_spi_storage;
static brfs_sdcard_storage_t    fs_sd_storage;

/* ---- BRFS file operations for VFS ---- */

static int fs_read(struct open_file *f, void *buf, int count)
{
    int brfs_fd;
    int is_sd;
    struct brfs_state *fs;

    brfs_fd = (int)((unsigned int)f->private & 0xFFFF);
    is_sd   = (int)(((unsigned int)f->private >> 16) & 1);
    fs = is_sd ? &brfs_sd : &brfs_spi;

    return brfs_read(fs, brfs_fd, buf, (unsigned int)count);
}

static int fs_write(struct open_file *f, const void *buf, int count)
{
    int brfs_fd;
    int is_sd;
    struct brfs_state *fs;

    brfs_fd = (int)((unsigned int)f->private & 0xFFFF);
    is_sd   = (int)(((unsigned int)f->private >> 16) & 1);
    fs = is_sd ? &brfs_sd : &brfs_spi;

    return brfs_write(fs, brfs_fd, buf, (unsigned int)count);
}

static int fs_lseek(struct open_file *f, int offset, int whence)
{
    int brfs_fd;
    int is_sd;
    struct brfs_state *fs;

    brfs_fd = (int)((unsigned int)f->private & 0xFFFF);
    is_sd   = (int)(((unsigned int)f->private >> 16) & 1);
    fs = is_sd ? &brfs_sd : &brfs_spi;

    /* BRFS seek only supports absolute offset (no whence) */
    if (whence == 1)
    {
        /* SEEK_CUR: get current position, add offset */
        int cur;
        cur = brfs_tell(fs, brfs_fd);
        if (cur < 0) return -1;
        return brfs_seek(fs, brfs_fd, (unsigned int)(cur + offset));
    }
    else if (whence == 2)
    {
        /* SEEK_END: get file size, add offset */
        int size;
        size = brfs_file_size(fs, brfs_fd);
        if (size < 0) return -1;
        return brfs_seek(fs, brfs_fd, (unsigned int)(size + offset));
    }
    /* SEEK_SET */
    return brfs_seek(fs, brfs_fd, (unsigned int)offset);
}

static int fs_close_file(struct open_file *f)
{
    int brfs_fd;
    int is_sd;
    struct brfs_state *fs;

    brfs_fd = (int)((unsigned int)f->private & 0xFFFF);
    is_sd   = (int)(((unsigned int)f->private >> 16) & 1);
    fs = is_sd ? &brfs_sd : &brfs_spi;

    return brfs_close(fs, brfs_fd);
}

/* Exported so vfs.c can assign it to BRFS-backed files */
struct file_ops fs_file_ops = {
    fs_read,
    fs_write,
    fs_lseek,
    fs_close_file,
    0 /* no ioctl for files */
};

/* ---- Path routing ---- */

struct brfs_state *fs_for_path(const char *path, const char **rel_out)
{
    /* "/sdcard/" prefix → SD card filesystem */
    if (path[0] == '/' && path[1] == 's' && path[2] == 'd'
        && path[3] == 'c' && path[4] == 'a' && path[5] == 'r'
        && path[6] == 'd' && path[7] == '/')
    {
        if (!fs_sd_ready) return 0;
        *rel_out = &path[8]; /* Skip "/sdcard/" */
        return &brfs_sd;
    }

    /* Everything else → SPI flash */
    if (path[0] == '/')
        *rel_out = &path[1]; /* Skip leading "/" */
    else
        *rel_out = path;
    return &brfs_spi;
}

/* ---- Initialization ---- */

void fs_init(void)
{
    int result;

    fs_sd_ready = 0;

    /* Initialize SPI flash BRFS */
    brfs_storage_spi_flash_init(&fs_spi_storage, SPI_FLASH_1);
    result = brfs_init(&brfs_spi, &fs_spi_storage.base,
                       (unsigned int *)BRFS_SPI_CACHE_START,
                       (BRFS_SPI_CACHE_END - BRFS_SPI_CACHE_START) / sizeof(unsigned int));
    if (result != BRFS_OK)
    {
        kernel_panic("BRFS SPI init failed");
        return;
    }

    result = brfs_mount(&brfs_spi);
    if (result != BRFS_OK)
    {
        kernel_log("  SPI flash: mount failed (needs format?)\n");
    }

    /* Try to initialize SD card BRFS */
    {
        sd_card_info_t sd_info;
        if (sd_init(&sd_info) == SD_OK)
        {
            brfs_storage_sdcard_init(&fs_sd_storage);
            result = brfs_init(&brfs_sd, &fs_sd_storage.base,
                               (unsigned int *)BRFS_SD_CACHE_START,
                               (BRFS_SD_CACHE_END - BRFS_SD_CACHE_START) / sizeof(unsigned int));
            if (result == BRFS_OK)
            {
                result = brfs_mount(&brfs_sd);
                if (result == BRFS_OK)
                {
                    fs_sd_ready = 1;
                    kernel_log("  sd card mounted\n");
                }
                else
                {
                    kernel_log("  sd card mount failed\n");
                }
            }
        }
    }
}

int fs_format_spi(unsigned int blocks, unsigned int words_per_block,
                  const char *label, int full)
{
    int result;
    result = brfs_format(&brfs_spi, blocks, words_per_block, label, full);
    if (result != BRFS_OK) return result;
    result = brfs_sync(&brfs_spi);
    return result;
}

int fs_format_sd(unsigned int blocks, unsigned int words_per_block,
                 const char *label, int full)
{
    int result;
    if (!fs_sd_ready) return -1;
    result = brfs_format(&brfs_sd, blocks, words_per_block, label, full);
    if (result != BRFS_OK) return result;
    result = brfs_sync(&brfs_sd);
    return result;
}

void fs_sync_all(void)
{
    brfs_sync(&brfs_spi);
    if (fs_sd_ready)
        brfs_sync(&brfs_sd);
}
