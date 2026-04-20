#include "spi.h"
#include "spi_flash.h"
#include "dma.h"
#include "fpgc.h"

#define SPIFLASH_CMD_WRITE_ENABLE    0x06
#define SPIFLASH_CMD_WRITE_DISABLE   0x04
#define SPIFLASH_CMD_READ_STATUS_1   0x05
#define SPIFLASH_CMD_WRITE_STATUS    0x01
#define SPIFLASH_CMD_PAGE_PROGRAM    0x02
#define SPIFLASH_CMD_READ_DATA       0x03
#define SPIFLASH_CMD_SECTOR_ERASE    0x20
#define SPIFLASH_CMD_BLOCK_ERASE_32K 0x52
#define SPIFLASH_CMD_BLOCK_ERASE_64K 0xD8
#define SPIFLASH_CMD_CHIP_ERASE      0xC7
#define SPIFLASH_CMD_JEDEC_ID        0x9F
#define SPIFLASH_CMD_UNIQUE_ID       0x4B
#define SPIFLASH_STATUS_BUSY         0x01
#define SPIFLASH_PAGE_SIZE           256
#define SPIFLASH_DUMMY               0x00

void
spi_flash_read_jedec_id(int spi_id, int *manufacturer_id, int *memory_type, int *capacity)
{
    spi_select(spi_id);
    spi_transfer(spi_id, SPIFLASH_CMD_JEDEC_ID);
    *manufacturer_id = spi_transfer(spi_id, SPIFLASH_DUMMY);
    *memory_type     = spi_transfer(spi_id, SPIFLASH_DUMMY);
    *capacity        = spi_transfer(spi_id, SPIFLASH_DUMMY);
    spi_deselect(spi_id);
}

void
spi_flash_enable_write(int spi_id)
{
    spi_select(spi_id);
    spi_transfer(spi_id, SPIFLASH_CMD_WRITE_ENABLE);
    spi_deselect(spi_id);
}

void
spi_flash_disable_write(int spi_id)
{
    spi_select(spi_id);
    spi_transfer(spi_id, SPIFLASH_CMD_WRITE_DISABLE);
    spi_deselect(spi_id);
}

int
spi_flash_read_status(int spi_id)
{
    int status;
    spi_select(spi_id);
    spi_transfer(spi_id, SPIFLASH_CMD_READ_STATUS_1);
    status = spi_transfer(spi_id, SPIFLASH_DUMMY);
    spi_deselect(spi_id);
    return status;
}

int
spi_flash_is_busy(int spi_id)
{
    return spi_flash_read_status(spi_id) & SPIFLASH_STATUS_BUSY;
}

void
spi_flash_wait_busy(int spi_id)
{
    while (spi_flash_is_busy(spi_id))
        ;
}

void
spi_flash_write_status(int spi_id, int status)
{
    spi_flash_enable_write(spi_id);
    spi_select(spi_id);
    spi_transfer(spi_id, SPIFLASH_CMD_WRITE_STATUS);
    spi_transfer(spi_id, status);
    spi_deselect(spi_id);
    spi_flash_wait_busy(spi_id);
}

static void
send_addr(int spi_id, int address)
{
    spi_transfer(spi_id, (address >> 16) & 0xFF);
    spi_transfer(spi_id, (address >> 8) & 0xFF);
    spi_transfer(spi_id, address & 0xFF);
}

void
spi_flash_write_page(int spi_id, int address, int *data, int length)
{
    int i;
    if (length > SPIFLASH_PAGE_SIZE)
        length = SPIFLASH_PAGE_SIZE;
    spi_flash_enable_write(spi_id);
    spi_select(spi_id);
    spi_transfer(spi_id, SPIFLASH_CMD_PAGE_PROGRAM);
    send_addr(spi_id, address);
    for (i = 0; i < length; i++)
        spi_transfer(spi_id, data[i] & 0xFF);
    spi_deselect(spi_id);
    spi_flash_wait_busy(spi_id);
}

void
spi_flash_read_data(int spi_id, int address, int *buffer, int length)
{
    int i;
    spi_select(spi_id);
    spi_transfer(spi_id, SPIFLASH_CMD_READ_DATA);
    send_addr(spi_id, address);
    for (i = 0; i < length; i++)
        buffer[i] = spi_transfer(spi_id, SPIFLASH_DUMMY);
    spi_deselect(spi_id);
}

static void
erase_cmd(int spi_id, int cmd, int address)
{
    spi_flash_enable_write(spi_id);
    spi_select(spi_id);
    spi_transfer(spi_id, cmd);
    send_addr(spi_id, address);
    spi_deselect(spi_id);
    spi_flash_wait_busy(spi_id);
}

void spi_flash_erase_sector(int spi_id, int address)   { erase_cmd(spi_id, SPIFLASH_CMD_SECTOR_ERASE, address); }
void spi_flash_erase_block_32k(int spi_id, int address) { erase_cmd(spi_id, SPIFLASH_CMD_BLOCK_ERASE_32K, address); }
void spi_flash_erase_block_64k(int spi_id, int address) { erase_cmd(spi_id, SPIFLASH_CMD_BLOCK_ERASE_64K, address); }

void
spi_flash_erase_chip(int spi_id)
{
    spi_flash_enable_write(spi_id);
    spi_select(spi_id);
    spi_transfer(spi_id, SPIFLASH_CMD_CHIP_ERASE);
    spi_deselect(spi_id);
    spi_flash_wait_busy(spi_id);
}

void
spi_flash_read_unique_id(int spi_id, int *id_buffer)
{
    int i;
    spi_select(spi_id);
    spi_transfer(spi_id, SPIFLASH_CMD_UNIQUE_ID);
    for (i = 0; i < 4; i++)
        spi_transfer(spi_id, SPIFLASH_DUMMY);
    for (i = 0; i < 8; i++)
        id_buffer[i] = spi_transfer(spi_id, SPIFLASH_DUMMY);
    spi_deselect(spi_id);
}

void
spi_flash_write_words(int spi_id, int address, unsigned int *data, int word_count)
{
    int i;
    unsigned int word;
    unsigned int byte_count;
    if (word_count > 64)
        word_count = 64;
    spi_flash_enable_write(spi_id);
    spi_select(spi_id);
    spi_transfer(spi_id, SPIFLASH_CMD_PAGE_PROGRAM);
    send_addr(spi_id, address);
    /*
     * Fast path: when the source buffer and byte count are 32-byte aligned
     * and the SPI controller supports DMA (id 0, 1, or 4), push the payload
     * with the DMAengine in MEM2SPI mode while the CPU is free.
     */
    byte_count = (unsigned int)word_count * 4u;
    if ((spi_id == 0 || spi_id == 1 || spi_id == 4) &&
        ((unsigned int)data % 32u == 0u) &&
        (byte_count % 32u == 0u) &&
        byte_count > 0u) {
        cache_flush_data();
        dma_start_spi(DMA_MEM2SPI, spi_id, 0u, (unsigned int)data, byte_count);
        while (dma_busy())
            ;
        (void)dma_status();
    } else {
        for (i = 0; i < word_count; i++) {
            word = data[i];
            /* Little-endian on disk: LSB first. */
            spi_transfer(spi_id, word & 0xFF);
            spi_transfer(spi_id, (word >> 8) & 0xFF);
            spi_transfer(spi_id, (word >> 16) & 0xFF);
            spi_transfer(spi_id, (word >> 24) & 0xFF);
        }
    }
    spi_deselect(spi_id);
    spi_flash_wait_busy(spi_id);
}

void
spi_flash_read_words(int spi_id, int address, unsigned int *buffer, int word_count)
{
    int i;
    unsigned int b0, b1, b2, b3;
    unsigned int byte_count;
    spi_select(spi_id);
    spi_transfer(spi_id, SPIFLASH_CMD_READ_DATA);
    send_addr(spi_id, address);
    /*
     * Fast path: aligned destination buffer and byte count -- pull the
     * payload via DMA SPI2MEM (available on SPI controllers 0, 1, and 4).
     */
    byte_count = (unsigned int)word_count * 4u;
    if ((spi_id == 0 || spi_id == 1 || spi_id == 4) &&
        ((unsigned int)buffer % 32u == 0u) &&
        (byte_count % 32u == 0u) &&
        byte_count > 0u) {
        cache_flush_data();
        dma_start_spi(DMA_SPI2MEM, spi_id, (unsigned int)buffer, 0u, byte_count);
        while (dma_busy())
            ;
        (void)dma_status();
        cache_flush_data();
    } else {
        for (i = 0; i < word_count; i++) {
            /* Little-endian on disk: LSB first. */
            b0 = spi_transfer(spi_id, SPIFLASH_DUMMY);
            b1 = spi_transfer(spi_id, SPIFLASH_DUMMY);
            b2 = spi_transfer(spi_id, SPIFLASH_DUMMY);
            b3 = spi_transfer(spi_id, SPIFLASH_DUMMY);
            buffer[i] = b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);
        }
    }
    spi_deselect(spi_id);
}
