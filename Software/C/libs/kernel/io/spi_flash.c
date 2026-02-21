//
// spi_flash library implementation.
//

#include "libs/kernel/io/spi_flash.h"
#include "libs/kernel/io/spi.h"

// SPI Flash Command Definitions
#define SPIFLASH_CMD_WRITE_ENABLE 0x06
#define SPIFLASH_CMD_WRITE_DISABLE 0x04
#define SPIFLASH_CMD_READ_STATUS_1 0x05
#define SPIFLASH_CMD_READ_STATUS_2 0x35
#define SPIFLASH_CMD_WRITE_STATUS 0x01
#define SPIFLASH_CMD_PAGE_PROGRAM 0x02
#define SPIFLASH_CMD_READ_DATA 0x03
#define SPIFLASH_CMD_FAST_READ 0x0B
#define SPIFLASH_CMD_SECTOR_ERASE 0x20
#define SPIFLASH_CMD_BLOCK_ERASE_32K 0x52
#define SPIFLASH_CMD_BLOCK_ERASE_64K 0xD8
#define SPIFLASH_CMD_CHIP_ERASE 0xC7
#define SPIFLASH_CMD_SUSPEND 0x75
#define SPIFLASH_CMD_RESUME 0x7A
#define SPIFLASH_CMD_POWER_DOWN 0xB9
#define SPIFLASH_CMD_RELEASE_POWER_DOWN 0xAB
#define SPIFLASH_CMD_JEDEC_ID 0x9F
#define SPIFLASH_CMD_MANUFACTURER_ID 0x90
#define SPIFLASH_CMD_UNIQUE_ID 0x4B

// Status Register Bits
#define SPIFLASH_STATUS_BUSY 0x01
#define SPIFLASH_STATUS_WEL 0x02
#define SPIFLASH_STATUS_BP0 0x04
#define SPIFLASH_STATUS_BP1 0x08
#define SPIFLASH_STATUS_BP2 0x10
#define SPIFLASH_STATUS_TB 0x20
#define SPIFLASH_STATUS_SEC 0x40
#define SPIFLASH_STATUS_SRP 0x80

// Winbond Manufacturer ID
#define SPIFLASH_MANID_WINBOND 0xEF

// Page size
#define SPIFLASH_PAGE_SIZE 256

// Dummy byte for reads
#define SPIFLASH_DUMMY_BYTE 0x00

// Read JEDEC manufacturer/type/capacity identifiers.
void spi_flash_read_jedec_id(int spi_id, int *manufacturer_id, int *memory_type, int *capacity)
{
  spi_select(spi_id);

  spi_transfer(spi_id, SPIFLASH_CMD_JEDEC_ID);

  *manufacturer_id = spi_transfer(spi_id, SPIFLASH_DUMMY_BYTE);
  *memory_type = spi_transfer(spi_id, SPIFLASH_DUMMY_BYTE);
  *capacity = spi_transfer(spi_id, SPIFLASH_DUMMY_BYTE);

  spi_deselect(spi_id);
}

// Set write-enable latch for erase/program operations.
void spi_flash_enable_write(int spi_id)
{
  spi_select(spi_id);
  spi_transfer(spi_id, SPIFLASH_CMD_WRITE_ENABLE);
  spi_deselect(spi_id);
}

// Clear write-enable latch.
void spi_flash_disable_write(int spi_id)
{
  spi_select(spi_id);
  spi_transfer(spi_id, SPIFLASH_CMD_WRITE_DISABLE);
  spi_deselect(spi_id);
}

// Read status register 1.
int spi_flash_read_status(int spi_id)
{
  int status;

  spi_select(spi_id);
  spi_transfer(spi_id, SPIFLASH_CMD_READ_STATUS_1);
  status = spi_transfer(spi_id, SPIFLASH_DUMMY_BYTE);
  spi_deselect(spi_id);

  return status;
}

// Return non-zero while the flash is busy.
int spi_flash_is_busy(int spi_id)
{
  return spi_flash_read_status(spi_id) & SPIFLASH_STATUS_BUSY;
}

// Block until the flash reports idle.
void spi_flash_wait_busy(int spi_id)
{
  while (spi_flash_is_busy(spi_id))
  {
    // Poll until not busy
  }
}

// Write status register bits.
void spi_flash_write_status(int spi_id, int status)
{
  spi_flash_enable_write(spi_id);

  spi_select(spi_id);
  spi_transfer(spi_id, SPIFLASH_CMD_WRITE_STATUS);
  spi_transfer(spi_id, status);
  spi_deselect(spi_id);

  spi_flash_wait_busy(spi_id);
}

// Program up to one page (256 bytes).
void spi_flash_write_page(int spi_id, int address, int *data, int length)
{
  int i;

  if (length > SPIFLASH_PAGE_SIZE)
  {
    length = SPIFLASH_PAGE_SIZE;
  }

  spi_flash_enable_write(spi_id);

  spi_select(spi_id);

  spi_transfer(spi_id, SPIFLASH_CMD_PAGE_PROGRAM);

  spi_transfer(spi_id, (address >> 16) & 0xFF);
  spi_transfer(spi_id, (address >> 8) & 0xFF);
  spi_transfer(spi_id, address & 0xFF);

  for (i = 0; i < length; i++)
  {
    spi_transfer(spi_id, data[i] & 0xFF);
  }

  spi_deselect(spi_id);

  spi_flash_wait_busy(spi_id);
}

// Read raw bytes starting at a 24-bit address.
void spi_flash_read_data(int spi_id, int address, int *buffer, int length)
{
  int i;

  spi_select(spi_id);

  spi_transfer(spi_id, SPIFLASH_CMD_READ_DATA);

  spi_transfer(spi_id, (address >> 16) & 0xFF);
  spi_transfer(spi_id, (address >> 8) & 0xFF);
  spi_transfer(spi_id, address & 0xFF);

  for (i = 0; i < length; i++)
  {
    buffer[i] = spi_transfer(spi_id, SPIFLASH_DUMMY_BYTE);
  }

  spi_deselect(spi_id);
}

// Erase a 4KB sector.
void spi_flash_erase_sector(int spi_id, int address)
{
  spi_flash_enable_write(spi_id);

  spi_select(spi_id);

  spi_transfer(spi_id, SPIFLASH_CMD_SECTOR_ERASE);

  spi_transfer(spi_id, (address >> 16) & 0xFF);
  spi_transfer(spi_id, (address >> 8) & 0xFF);
  spi_transfer(spi_id, address & 0xFF);

  spi_deselect(spi_id);

  spi_flash_wait_busy(spi_id);
}

// Erase a 32KB block.
void spi_flash_erase_block_32k(int spi_id, int address)
{
  spi_flash_enable_write(spi_id);

  spi_select(spi_id);

  spi_transfer(spi_id, SPIFLASH_CMD_BLOCK_ERASE_32K);

  spi_transfer(spi_id, (address >> 16) & 0xFF);
  spi_transfer(spi_id, (address >> 8) & 0xFF);
  spi_transfer(spi_id, address & 0xFF);

  spi_deselect(spi_id);

  spi_flash_wait_busy(spi_id);
}

// Erase a 64KB block.
void spi_flash_erase_block_64k(int spi_id, int address)
{
  spi_flash_enable_write(spi_id);

  spi_select(spi_id);

  spi_transfer(spi_id, SPIFLASH_CMD_BLOCK_ERASE_64K);

  spi_transfer(spi_id, (address >> 16) & 0xFF);
  spi_transfer(spi_id, (address >> 8) & 0xFF);
  spi_transfer(spi_id, address & 0xFF);

  spi_deselect(spi_id);

  spi_flash_wait_busy(spi_id);
}

// Erase the entire flash chip.
void spi_flash_erase_chip(int spi_id)
{
  spi_flash_enable_write(spi_id);

  spi_select(spi_id);

  spi_transfer(spi_id, SPIFLASH_CMD_CHIP_ERASE);

  spi_deselect(spi_id);

  spi_flash_wait_busy(spi_id);
}

// Read the 64-bit unique identifier.
void spi_flash_read_unique_id(int spi_id, int *id_buffer)
{
  int i;

  spi_select(spi_id);

  spi_transfer(spi_id, SPIFLASH_CMD_UNIQUE_ID);

  spi_transfer(spi_id, SPIFLASH_DUMMY_BYTE);
  spi_transfer(spi_id, SPIFLASH_DUMMY_BYTE);
  spi_transfer(spi_id, SPIFLASH_DUMMY_BYTE);
  spi_transfer(spi_id, SPIFLASH_DUMMY_BYTE);

  for (i = 0; i < 8; i++)
  {
    id_buffer[i] = spi_transfer(spi_id, SPIFLASH_DUMMY_BYTE);
  }

  spi_deselect(spi_id);
}

// Program up to 64 32-bit words at one page address.
void spi_flash_write_words(int spi_id, int address, unsigned int *data, int word_count)
{
  int i;
  unsigned int word;

  if (word_count > 64)
  {
    word_count = 64;
  }

  spi_flash_enable_write(spi_id);

  spi_select(spi_id);

  spi_transfer(spi_id, SPIFLASH_CMD_PAGE_PROGRAM);

  spi_transfer(spi_id, (address >> 16) & 0xFF);
  spi_transfer(spi_id, (address >> 8) & 0xFF);
  spi_transfer(spi_id, address & 0xFF);

  for (i = 0; i < word_count; i++)
  {
    word = data[i];
    spi_transfer(spi_id, (word >> 24) & 0xFF);
    spi_transfer(spi_id, (word >> 16) & 0xFF);
    spi_transfer(spi_id, (word >> 8) & 0xFF);
    spi_transfer(spi_id, word & 0xFF);
  }

  spi_deselect(spi_id);

  spi_flash_wait_busy(spi_id);
}

// Read 32-bit words in big-endian byte order.
void spi_flash_read_words(int spi_id, int address, unsigned int *buffer, int word_count)
{
  int i;
  unsigned int b0;
  unsigned int b1;
  unsigned int b2;
  unsigned int b3;

  spi_select(spi_id);

  spi_transfer(spi_id, SPIFLASH_CMD_READ_DATA);

  spi_transfer(spi_id, (address >> 16) & 0xFF);
  spi_transfer(spi_id, (address >> 8) & 0xFF);
  spi_transfer(spi_id, address & 0xFF);

  for (i = 0; i < word_count; i++)
  {
    b0 = spi_transfer(spi_id, SPIFLASH_DUMMY_BYTE);
    b1 = spi_transfer(spi_id, SPIFLASH_DUMMY_BYTE);
    b2 = spi_transfer(spi_id, SPIFLASH_DUMMY_BYTE);
    b3 = spi_transfer(spi_id, SPIFLASH_DUMMY_BYTE);
    buffer[i] = (b0 << 24) | (b1 << 16) | (b2 << 8) | b3;
  }

  spi_deselect(spi_id);
}
