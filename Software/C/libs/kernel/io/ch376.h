#ifndef CH376_H
#define CH376_H

/*
 * Library for the CH376 USB interface chip.
 * Provides generic USB host functionality (like CH375).
 * Does not support device mode nor the FAT file system internals.
 * Builds on top of the SPI library.
 */

#define CH376_SPI_TOP       SPI_ID_USB_0
#define CH376_SPI_BOTTOM    SPI_ID_USB_1

#endif // CH376_H
