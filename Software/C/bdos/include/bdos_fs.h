#ifndef BDOS_FS_H
#define BDOS_FS_H

/* BRFS flash target */
#define BDOS_FS_FLASH_ID SPI_FLASH_1

/* Shared filesystem state — SPI flash */
extern struct brfs_state brfs_spi;
extern int bdos_fs_ready;
extern int bdos_fs_boot_needs_format;
extern int bdos_fs_last_mount_error;

/* Shared filesystem state — SD card */
extern struct brfs_state brfs_sd;
extern int bdos_sd_ready;

/* Filesystem functions */
void bdos_fs_boot_init(void);
void bdos_fs_sd_init(void);
int bdos_fs_format_and_sync(unsigned int total_blocks, unsigned int words_per_block,
                            char *label, int full_format);
int bdos_fs_sd_format_and_sync(unsigned int total_blocks, unsigned int words_per_block,
                               char *label, int full_format);
int bdos_fs_sync_now(void);
char *bdos_fs_error_string(int error_code);

#endif /* BDOS_FS_H */
