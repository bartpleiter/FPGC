/*
 * bmp.c — 8-bit greyscale BMP encoder/decoder for camera captures
 *
 * Saves VRAMPX (320×240 bytes, Y luminance) as a Windows BMP with
 * an 8-bit greyscale palette (256 entries). BMP stores rows bottom
 * to top, so we flip vertically when saving/loading.
 *
 * File layout:
 *   [0..13]    BITMAPFILEHEADER  (14 bytes)
 *   [14..53]   BITMAPINFOHEADER  (40 bytes)
 *   [54..1077] Palette (256 × 4 bytes = 1024 bytes)
 *   [1078..]   Pixel data (320 × 240 = 76800 bytes, bottom-to-top)
 */
#include "bmp.h"
#include "fpgc.h"

/* Write a 16-bit little-endian value into buf */
static void put_le16(unsigned char *buf, unsigned int val)
{
    buf[0] = (unsigned char)(val & 0xFF);
    buf[1] = (unsigned char)((val >> 8) & 0xFF);
}

/* Write a 32-bit little-endian value into buf */
static void put_le32(unsigned char *buf, unsigned int val)
{
    buf[0] = (unsigned char)(val & 0xFF);
    buf[1] = (unsigned char)((val >> 8) & 0xFF);
    buf[2] = (unsigned char)((val >> 16) & 0xFF);
    buf[3] = (unsigned char)((val >> 24) & 0xFF);
}

/* Read a 32-bit little-endian value from buf */
static unsigned int get_le32(const unsigned char *buf)
{
    return (unsigned int)buf[0]
         | ((unsigned int)buf[1] << 8)
         | ((unsigned int)buf[2] << 16)
         | ((unsigned int)buf[3] << 24);
}

int bmp_save(struct brfs_state *fs, const char *path,
             unsigned int src_addr, int width, int height)
{
    unsigned char hdr[BMP_HEADER_TOTAL];
    int fd;
    int rc;
    int row;
    int i;
    unsigned char rowbuf[BMP_WIDTH];
    int pixels;
    int file_size;

    pixels = width * height;

    /* Create the file */
    rc = brfs_create_file(fs, path);
    if (rc != BRFS_OK && rc != BRFS_ERR_EXISTS) return -1;

    fd = brfs_open(fs, path);
    if (fd < 0) return -2;

    /* Build BITMAPFILEHEADER (14 bytes) */
    for (i = 0; i < BMP_HEADER_TOTAL; i++) hdr[i] = 0;

    file_size = BMP_HEADER_TOTAL + pixels;

    hdr[0] = 'B';
    hdr[1] = 'M';
    put_le32(&hdr[2], (unsigned int)file_size); /* bfSize */
    /* hdr[6..9] = 0 (reserved) */
    put_le32(&hdr[10], BMP_HEADER_TOTAL); /* bfOffBits */

    /* Build BITMAPINFOHEADER (40 bytes) at offset 14 */
    put_le32(&hdr[14], 40);                     /* biSize */
    put_le32(&hdr[18], (unsigned int)width);     /* biWidth */
    put_le32(&hdr[22], (unsigned int)height);    /* biHeight (positive = bottom-up) */
    put_le16(&hdr[26], 1);               /* biPlanes */
    put_le16(&hdr[28], 8);               /* biBitCount */
    /* hdr[30..33] = 0 (biCompression = BI_RGB) */
    put_le32(&hdr[34], (unsigned int)pixels);    /* biSizeImage */
    put_le32(&hdr[38], 2835);            /* biXPelsPerMeter (72 dpi) */
    put_le32(&hdr[42], 2835);            /* biYPelsPerMeter */
    put_le32(&hdr[46], 256);             /* biClrUsed */
    put_le32(&hdr[50], 256);             /* biClrImportant */

    /* Build greyscale palette (256 entries × 4 bytes: B, G, R, 0) */
    for (i = 0; i < 256; i++) {
        int ofs;
        ofs = 54 + i * 4;
        hdr[ofs + 0] = (unsigned char)i; /* Blue */
        hdr[ofs + 1] = (unsigned char)i; /* Green */
        hdr[ofs + 2] = (unsigned char)i; /* Red */
        hdr[ofs + 3] = 0;               /* Reserved */
    }

    /* Write header + palette */
    rc = brfs_write(fs, fd, hdr, BMP_HEADER_TOTAL);
    if (rc < 0) { brfs_close(fs, fd); return -3; }

    /* Write pixel data bottom-to-top (BMP row order) */
    for (row = height - 1; row >= 0; row--) {
        /* Read one row from capture buffer in SDRAM (byte access) */
        unsigned char *src_row;
        src_row = (unsigned char *)(src_addr + row * width);
        for (i = 0; i < width; i++) {
            rowbuf[i] = src_row[i];
        }
        rc = brfs_write(fs, fd, rowbuf, (unsigned int)width);
        if (rc < 0) { brfs_close(fs, fd); return -4; }
    }

    brfs_close(fs, fd);
    return 0;
}

int bmp_load_to_screen(struct brfs_state *fs, const char *path)
{
    unsigned char hdr[BMP_HEADER_TOTAL];
    int fd;
    int rc;
    int row;
    int i;
    int width;
    int height;
    int upscale;
    unsigned char rowbuf[BMP_WIDTH];

    fd = brfs_open(fs, path);
    if (fd < 0) return -1;

    /* Read header + palette */
    rc = brfs_read(fs, fd, hdr, BMP_HEADER_TOTAL);
    if (rc < BMP_HEADER_TOTAL) { brfs_close(fs, fd); return -2; }

    /* Validate BMP signature */
    if (hdr[0] != 'B' || hdr[1] != 'M') { brfs_close(fs, fd); return -3; }

    /* Get dimensions */
    width = (int)get_le32(&hdr[18]);
    height = (int)get_le32(&hdr[22]);

    /* Support QVGA (320×240) and QQVGA (160×120) */
    upscale = 0;
    if (width == 160 && height == 120) {
        upscale = 1;
    } else if (width != BMP_WIDTH || height != BMP_HEIGHT) {
        brfs_close(fs, fd);
        return -4;
    }

    /* Read pixel data (bottom-to-top) and write to VRAMPX */
    for (row = height - 1; row >= 0; row--) {
        rc = brfs_read(fs, fd, rowbuf, (unsigned int)width);
        if (rc < width) { brfs_close(fs, fd); return -5; }

        if (upscale) {
            /* 2× pixel doubling for QQVGA → QVGA display */
            int dy;
            dy = row * 2;
            for (i = 0; i < width; i++) {
                unsigned int pix;
                int dx;
                pix = (unsigned int)rowbuf[i];
                dx = i * 2;
                __builtin_store(FPGC_GPU_PIXEL_DATA + dy * BMP_WIDTH + dx, pix);
                __builtin_store(FPGC_GPU_PIXEL_DATA + dy * BMP_WIDTH + dx + 1, pix);
                __builtin_store(FPGC_GPU_PIXEL_DATA + (dy + 1) * BMP_WIDTH + dx, pix);
                __builtin_store(FPGC_GPU_PIXEL_DATA + (dy + 1) * BMP_WIDTH + dx + 1, pix);
            }
        } else {
            for (i = 0; i < width; i++) {
                __builtin_store(FPGC_GPU_PIXEL_DATA + row * BMP_WIDTH + i,
                                (unsigned int)rowbuf[i]);
            }
        }
    }

    brfs_close(fs, fd);
    return 0;
}
