#ifndef SPI_H
#define SPI_H

/*
 * SPI library for SPI communication
 * Provides functions to select/deselect the SPI device and transfer data.
 */

int spi_0_transfer(int data);
void spi_0_select();
void spi_0_deselect();

int spi_1_transfer(int data);
void spi_1_select();
void spi_1_deselect();

int spi_2_transfer(int data);
void spi_2_select();
void spi_2_deselect();

int spi_3_transfer(int data);
void spi_3_select();
void spi_3_deselect();

int spi_4_transfer(int data);
void spi_4_select();
void spi_4_deselect();

int spi_5_transfer(int data);
void spi_5_select();
void spi_5_deselect();

#endif // SPI_H
