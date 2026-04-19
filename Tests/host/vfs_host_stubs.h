/*
 * vfs_host_stubs.h — host replacement for "bdos.h" + "bdos_proc.h"
 * when compiling Software/C/bdos/vfs.c into a desktop gcc binary
 * for unit testing /dev/pixpal (and future device tests).
 *
 * The real bdos.h pulls libc + libfpgc + every kernel header. We only
 * need: bdos_vfs.h's struct definitions, a fake bdos_proc_current()
 * returning a static fd table, plus enough stubs that the BRFS / TTY
 * code paths inside vfs.c link (we never exercise those paths in this
 * test, so the stubs can return failures).
 */
#ifndef VFS_HOST_STUBS_H
#define VFS_HOST_STUBS_H

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "bdos_vfs.h"

/* ---- Pixel palette MMIO emulation -------------------------------- */
extern unsigned int g_fake_pixpal[256];

static inline void gpu_set_pixel_palette(unsigned int index,
                                         unsigned int rgb24)
{
    if (index < 256) g_fake_pixpal[index] = rgb24 & 0x00FFFFFF;
}

static inline unsigned int gpu_get_pixel_palette(unsigned int index)
{
    return (index < 256) ? (g_fake_pixpal[index] & 0x00FFFFFF) : 0;
}

/* ---- bdos_proc stub --------------------------------------------- */
typedef struct {
    bdos_fd_t fds[BDOS_FD_MAX];
} bdos_proc_t;

bdos_proc_t *bdos_proc_current(void);

/* ---- BRFS stubs (DEV_FILE never exercised in pixpal tests) ------- */
static inline int  brfs_open(const char *p)              { (void)p; return -1; }
static inline int  brfs_close(int h)                     { (void)h; return -1; }
static inline int  brfs_read(int h, void *b, unsigned n) { (void)h; (void)b; (void)n; return -1; }
static inline int  brfs_write(int h, const void *b, unsigned n) { (void)h; (void)b; (void)n; return -1; }
static inline int  brfs_seek(int h, unsigned p)          { (void)h; (void)p; return -1; }
static inline int  brfs_tell(int h)                      { (void)h; return -1; }
static inline int  brfs_file_size(int h)                 { (void)h; return -1; }
static inline int  brfs_exists(const char *p)            { (void)p; return 0; }
static inline int  brfs_create_file(const char *p)       { (void)p; return -1; }
static inline int  brfs_delete(const char *p)            { (void)p; return -1; }

/* ---- TTY stubs (DEV_TTY never exercised in pixpal tests) --------- */
static inline int bdos_keyboard_event_available(void) { return 0; }
static inline int bdos_keyboard_event_read(void)      { return -1; }
static inline void term_putchar(int c)                { (void)c; }

#endif /* VFS_HOST_STUBS_H */
