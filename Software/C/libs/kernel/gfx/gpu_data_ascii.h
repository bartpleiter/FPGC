#ifndef GPU_DATA_ASCII_H
#define GPU_DATA_ASCII_H

/*
 * Contains default ASCII table with default terminal style palette table.
 * Assembly is used as workaround to store them efficiently.
 * An offset of 3 words should be used to get to the actual data because of the function prologue.
 */

void DATA_PALETTE_DEFAULT(void);
void DATA_ASCII_DEFAULT(void);

#endif // GPU_DATA_ASCII_H
