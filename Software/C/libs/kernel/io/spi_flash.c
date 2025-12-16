#include "libs/kernel/io/spi_flash.h"
#include "libs/kernel/io/spi.h"

/* SPI Flash Command Definitions */
#define SPIFLASH_CMD_WRITE_ENABLE       0x06
#define SPIFLASH_CMD_WRITE_DISABLE      0x04
#define SPIFLASH_CMD_READ_STATUS_1      0x05
#define SPIFLASH_CMD_READ_STATUS_2      0x35
#define SPIFLASH_CMD_WRITE_STATUS       0x01
#define SPIFLASH_CMD_PAGE_PROGRAM       0x02
#define SPIFLASH_CMD_READ_DATA          0x03
#define SPIFLASH_CMD_FAST_READ          0x0B
#define SPIFLASH_CMD_SECTOR_ERASE       0x20
#define SPIFLASH_CMD_BLOCK_ERASE_32K    0x52
#define SPIFLASH_CMD_BLOCK_ERASE_64K    0xD8
#define SPIFLASH_CMD_CHIP_ERASE         0xC7
#define SPIFLASH_CMD_SUSPEND            0x75
#define SPIFLASH_CMD_RESUME             0x7A
#define SPIFLASH_CMD_POWER_DOWN         0xB9
#define SPIFLASH_CMD_RELEASE_POWER_DOWN 0xAB
#define SPIFLASH_CMD_JEDEC_ID           0x9F
#define SPIFLASH_CMD_MANUFACTURER_ID    0x90
#define SPIFLASH_CMD_UNIQUE_ID          0x4B

/* Status Register Bits */
#define SPIFLASH_STATUS_BUSY            0x01
#define SPIFLASH_STATUS_WEL             0x02
#define SPIFLASH_STATUS_BP0             0x04
#define SPIFLASH_STATUS_BP1             0x08
#define SPIFLASH_STATUS_BP2             0x10
#define SPIFLASH_STATUS_TB              0x20
#define SPIFLASH_STATUS_SEC             0x40
#define SPIFLASH_STATUS_SRP             0x80

/* Winbond Manufacturer ID */
#define SPIFLASH_MANID_WINBOND          0xEF

/* Page size */
#define SPIFLASH_PAGE_SIZE              256

/* Dummy byte for reads */
#define SPIFLASH_DUMMY_BYTE             0x00


void spi_flash_read_jedec_id(int spi_id, int* manufacturer_id, int* memory_type, int* capacity)
{
    spi_select(spi_id);
    
    /* Send JEDEC ID command */
    spi_transfer(spi_id, SPIFLASH_CMD_JEDEC_ID);
    
    /* Read 3 bytes of ID information */
    *manufacturer_id = spi_transfer(spi_id, SPIFLASH_DUMMY_BYTE);
    *memory_type = spi_transfer(spi_id, SPIFLASH_DUMMY_BYTE);
    *capacity = spi_transfer(spi_id, SPIFLASH_DUMMY_BYTE);
    
    spi_deselect(spi_id);
}

void spi_flash_enable_write(int spi_id)
{
    spi_select(spi_id);
    spi_transfer(spi_id, SPIFLASH_CMD_WRITE_ENABLE);
    spi_deselect(spi_id);
}

void spi_flash_disable_write(int spi_id)
{
    spi_select(spi_id);
    spi_transfer(spi_id, SPIFLASH_CMD_WRITE_DISABLE);
    spi_deselect(spi_id);
}

int spi_flash_read_status(int spi_id)
{
    int status;
    
    spi_select(spi_id);
    spi_transfer(spi_id, SPIFLASH_CMD_READ_STATUS_1);
    status = spi_transfer(spi_id, SPIFLASH_DUMMY_BYTE);
    spi_deselect(spi_id);
    
    return status;
}

int spi_flash_is_busy(int spi_id)
{
    return spi_flash_read_status(spi_id) & SPIFLASH_STATUS_BUSY;
}

void spi_flash_wait_busy(int spi_id)
{
    while (spi_flash_is_busy(spi_id))
    {
        /* Poll until not busy */
    }
}

void spi_flash_write_status(int spi_id, int status)
{
    spi_flash_enable_write(spi_id);
    
    spi_select(spi_id);
    spi_transfer(spi_id, SPIFLASH_CMD_WRITE_STATUS);
    spi_transfer(spi_id, status);
    spi_deselect(spi_id);
    
    spi_flash_wait_busy(spi_id);
}

void spi_flash_write_page(int spi_id, int address, int* data, int length)
{
    int i;
    
    /* Length should not exceed page size */
    if (length > SPIFLASH_PAGE_SIZE)
    {
        length = SPIFLASH_PAGE_SIZE;
    }
    
    /* Enable write operations */
    spi_flash_enable_write(spi_id);
    
    spi_select(spi_id);
    
    /* Send page program command */
    spi_transfer(spi_id, SPIFLASH_CMD_PAGE_PROGRAM);
    
    /* Send 24-bit address (big endian) */
    spi_transfer(spi_id, (address >> 16) & 0xFF);
    spi_transfer(spi_id, (address >> 8) & 0xFF);
    spi_transfer(spi_id, address & 0xFF);
    
    /* Write data bytes */
    for (i = 0; i < length; i++)
    {
        spi_transfer(spi_id, data[i] & 0xFF);
    }
    
    spi_deselect(spi_id);
    
    /* Wait for write to complete */
    spi_flash_wait_busy(spi_id);
}

void spi_flash_read_data(int spi_id, int address, int* buffer, int length)
{
    int i;
    
    spi_select(spi_id);
    
    /* Send read data command */
    spi_transfer(spi_id, SPIFLASH_CMD_READ_DATA);
    
    /* Send 24-bit address (big endian) */
    spi_transfer(spi_id, (address >> 16) & 0xFF);
    spi_transfer(spi_id, (address >> 8) & 0xFF);
    spi_transfer(spi_id, address & 0xFF);
    
    /* Read data bytes */
    for (i = 0; i < length; i++)
    {
        buffer[i] = spi_transfer(spi_id, SPIFLASH_DUMMY_BYTE);
    }
    
    spi_deselect(spi_id);
}

void spi_flash_erase_sector(int spi_id, int address)
{
    /* Enable write operations */
    spi_flash_enable_write(spi_id);
    
    spi_select(spi_id);
    
    /* Send sector erase command (4KB) */
    spi_transfer(spi_id, SPIFLASH_CMD_SECTOR_ERASE);
    
    /* Send 24-bit address (big endian) */
    spi_transfer(spi_id, (address >> 16) & 0xFF);
    spi_transfer(spi_id, (address >> 8) & 0xFF);
    spi_transfer(spi_id, address & 0xFF);
    
    spi_deselect(spi_id);
    
    /* Wait for erase to complete */
    spi_flash_wait_busy(spi_id);
}

void spi_flash_erase_block_32k(int spi_id, int address)
{
    /* Enable write operations */
    spi_flash_enable_write(spi_id);
    
    spi_select(spi_id);
    
    /* Send 32KB block erase command */
    spi_transfer(spi_id, SPIFLASH_CMD_BLOCK_ERASE_32K);
    
    /* Send 24-bit address (big endian) */
    spi_transfer(spi_id, (address >> 16) & 0xFF);
    spi_transfer(spi_id, (address >> 8) & 0xFF);
    spi_transfer(spi_id, address & 0xFF);
    
    spi_deselect(spi_id);
    
    /* Wait for erase to complete */
    spi_flash_wait_busy(spi_id);
}

void spi_flash_erase_block_64k(int spi_id, int address)
{
    /* Enable write operations */
    spi_flash_enable_write(spi_id);
    
    spi_select(spi_id);
    
    /* Send 64KB block erase command */
    spi_transfer(spi_id, SPIFLASH_CMD_BLOCK_ERASE_64K);
    
    /* Send 24-bit address (big endian) */
    spi_transfer(spi_id, (address >> 16) & 0xFF);
    spi_transfer(spi_id, (address >> 8) & 0xFF);
    spi_transfer(spi_id, address & 0xFF);
    
    spi_deselect(spi_id);
    
    /* Wait for erase to complete */
    spi_flash_wait_busy(spi_id);
}

void spi_flash_erase_chip(int spi_id)
{
    /* Enable write operations */
    spi_flash_enable_write(spi_id);
    
    spi_select(spi_id);
    
    /* Send chip erase command */
    spi_transfer(spi_id, SPIFLASH_CMD_CHIP_ERASE);
    
    spi_deselect(spi_id);
    
    /* Wait for erase to complete (this can take a long time) */
    spi_flash_wait_busy(spi_id);
}

void spi_flash_read_unique_id(int spi_id, int* id_buffer)
{
    int i;
    
    spi_select(spi_id);
    
    /* Send unique ID command */
    spi_transfer(spi_id, SPIFLASH_CMD_UNIQUE_ID);
    
    /* Send 4 dummy bytes */
    spi_transfer(spi_id, SPIFLASH_DUMMY_BYTE);
    spi_transfer(spi_id, SPIFLASH_DUMMY_BYTE);
    spi_transfer(spi_id, SPIFLASH_DUMMY_BYTE);
    spi_transfer(spi_id, SPIFLASH_DUMMY_BYTE);
    
    /* Read 8 bytes of unique ID */
    for (i = 0; i < 8; i++)
    {
        id_buffer[i] = spi_transfer(spi_id, SPIFLASH_DUMMY_BYTE);
    }
    
    spi_deselect(spi_id);
}
