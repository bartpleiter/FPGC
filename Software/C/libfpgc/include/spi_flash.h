#ifndef FPGC_SPI_FLASH_H
#define FPGC_SPI_FLASH_H

void spi_flash_read_jedec_id(int spi_id, int *manufacturer_id, int *memory_type, int *capacity);
void spi_flash_enable_write(int spi_id);
void spi_flash_disable_write(int spi_id);
int  spi_flash_read_status(int spi_id);
int  spi_flash_is_busy(int spi_id);
void spi_flash_wait_busy(int spi_id);
void spi_flash_write_status(int spi_id, int status);
void spi_flash_write_page(int spi_id, int address, int *data, int length);
void spi_flash_read_data(int spi_id, int address, int *buffer, int length);
void spi_flash_erase_sector(int spi_id, int address);
void spi_flash_erase_block_32k(int spi_id, int address);
void spi_flash_erase_block_64k(int spi_id, int address);
void spi_flash_erase_chip(int spi_id);
void spi_flash_read_unique_id(int spi_id, int *id_buffer);
void spi_flash_write_words(int spi_id, int address, unsigned int *data, int word_count);
void spi_flash_read_words(int spi_id, int address, unsigned int *buffer, int word_count);

#endif /* FPGC_SPI_FLASH_H */
