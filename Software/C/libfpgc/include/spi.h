#ifndef FPGC_SPI_H
#define FPGC_SPI_H

/* SPI bus IDs */
#define SPI_FLASH_0     0
#define SPI_FLASH_1     1
#define SPI_USB_0       2
#define SPI_USB_1       3
#define SPI_ETH         4
#define SPI_SD_CARD     5
#define SPI_COUNT       6

/* Transfer one byte on the selected SPI bus, returns received byte */
int  spi_transfer(int spi_id, int data);

/* Assert chip select (active low) */
void spi_select(int spi_id);

/* Deassert chip select */
void spi_deselect(int spi_id);

#endif /* FPGC_SPI_H */
