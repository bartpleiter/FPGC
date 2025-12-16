#ifndef SPI_FLASH_H
#define SPI_FLASH_H

/*
 * Library for SPI flash memory operations
 * Builds on top of the SPI library to provide flash-specific functions.
 */

#define SPI_FLASH_0 0
#define SPI_FLASH_1 1

// Read JEDEC ID
void spi_flash_read_jedec_id(int spi_id, int* manufacturer_id, int* memory_type, int* capacity);

// Enable write operations
void spi_flash_enable_write(int spi_id);

// Page programming (256 bytes max per page)
void spi_flash_write_page(int spi_id, int address, int* data, int length);

// Read data (any length, any address)
void spi_flash_read_data(int spi_id, int address, int* buffer, int length);

// Erase 4KB sector
void spi_flash_erase_sector(int spi_id, int address);

// Erase 32KB block
void spi_flash_erase_block_32k(int spi_id, int address);

// Erase 64KB block
void spi_flash_erase_block_64k(int spi_id, int address);

// Erase entire chip
void spi_flash_erase_chip(int spi_id);

// Read status register
int spi_flash_read_status(int spi_id);

// Wait until device is not busy (poll status register)
void spi_flash_wait_busy(int spi_id);

// Check if write is in progress
int spi_flash_is_busy(int spi_id);

// Disable write
void spi_flash_disable_write(int spi_id);

// Read/write status register for block protection
void spi_flash_write_status(int spi_id, int status);

// Read unique ID
void spi_flash_read_unique_id(int spi_id, int* id_buffer);

#endif // SPI_FLASH_H
