#ifndef SPI_H
#define SPI_H

// SPI library for SPI communication
// Provides functions to select/deselect the SPI device and transfer data.

#define SPI_ID_FLASH_0 0
#define SPI_ID_FLASH_1 1
#define SPI_ID_USB_0 2
#define SPI_ID_USB_1 3
#define SPI_ID_ETH 4
#define SPI_ID_SD_CARD 5

int spi_transfer(int spi_id, int data);
void spi_select(int spi_id);
void spi_deselect(int spi_id);

#endif // SPI_H
