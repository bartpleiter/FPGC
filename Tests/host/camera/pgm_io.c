/*
 * FPGC-Camera — PGM Image I/O Implementation
 */

#include "pgm_io.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

u8 *pgm_read(const char *path, int *w, int *h)
{
    FILE *f;
    char magic[3];
    int maxval, c;
    u8 *buf;

    f = fopen(path, "rb");
    if (!f) return NULL;

    /* Read magic number */
    if (fscanf(f, "%2s", magic) != 1 || strcmp(magic, "P5") != 0) {
        fclose(f);
        return NULL;
    }

    /* Skip comments */
    while ((c = fgetc(f)) != EOF) {
        if (c == '#') {
            while ((c = fgetc(f)) != EOF && c != '\n') {}
        } else if (c > ' ') {
            ungetc(c, f);
            break;
        }
    }

    /* Read dimensions and max value */
    if (fscanf(f, "%d %d %d", w, h, &maxval) != 3) {
        fclose(f);
        return NULL;
    }

    /* Skip single whitespace after maxval */
    fgetc(f);

    /* Read pixel data */
    buf = (u8 *)malloc(*w * *h);
    if (!buf) {
        fclose(f);
        return NULL;
    }

    if ((int)fread(buf, 1, *w * *h, f) != *w * *h) {
        free(buf);
        fclose(f);
        return NULL;
    }

    fclose(f);
    return buf;
}

int pgm_write(const char *path, const u8 *buf, int w, int h)
{
    FILE *f = fopen(path, "wb");
    if (!f) return -1;

    fprintf(f, "P5\n%d %d\n255\n", w, h);
    if ((int)fwrite(buf, 1, w * h, f) != w * h) {
        fclose(f);
        return -1;
    }

    fclose(f);
    return 0;
}
