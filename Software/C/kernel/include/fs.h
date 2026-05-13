/*
 * fs.h — BRFS filesystem mount and path routing.
 *
 * Two BRFS instances: SPI flash (root /) and SD card (/sdcard).
 */
#ifndef KERNEL_FS_H
#define KERNEL_FS_H

#include "brfs.h"

/* BRFS instances (globals for direct access by fs.c) */
extern struct brfs_state brfs_spi;
extern struct brfs_state brfs_sd;
extern int fs_sd_ready;

/* Initialize and mount filesystems. */
void fs_init(void);

/* Route a path to the correct BRFS instance.
 * Sets *rel_path to the path relative to the mount point.
 * Returns the brfs_state pointer. */
struct brfs_state *fs_for_path(const char *path, const char **rel_path);

/* Format + sync */
int fs_format_spi(unsigned int blocks, unsigned int words_per_block,
                  const char *label, int full);
int fs_format_sd(unsigned int blocks, unsigned int words_per_block,
                 const char *label, int full);

/* Sync all mounted filesystems */
void fs_sync_all(void);

#endif /* KERNEL_FS_H */
