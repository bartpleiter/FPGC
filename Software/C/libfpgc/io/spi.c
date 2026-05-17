/*
 * SPI bus abstraction layer
 *
 * Provides uniform access to all 6 SPI buses (SPI0–SPI5).
 * Each bus has a DATA register and a CS register in MMIO space.
 *
 * Public API:
 *   spi_transfer(spi_id, data)   -> int (received byte)
 *   spi_select(spi_id)           -> void (assert CS low)
 *   spi_deselect(spi_id)         -> void (release CS high)
 *
 * Bus assignments: see fpgc.h (FPGC_SPI_FLASH_0..FPGC_SPI_SD_CARD)
 * Dependencies: fpgc.h
 * Build: part of libfpgc (make compile-kernel)
 */
#include "fpgc.h"
#include "spi.h"

/* Register address tables (data register, chip select register) */
static const int spi_data_addr[SPI_COUNT] = {
    FPGC_SPI0_DATA, FPGC_SPI1_DATA, FPGC_SPI2_DATA,
    FPGC_SPI3_DATA, FPGC_SPI4_DATA, FPGC_SPI5_DATA
};

static const int spi_cs_addr[SPI_COUNT] = {
    FPGC_SPI0_CS, FPGC_SPI1_CS, FPGC_SPI2_CS,
    FPGC_SPI3_CS, FPGC_SPI4_CS, FPGC_SPI5_CS
};

int
spi_transfer(int spi_id, int data)
{
    if (spi_id < 0 || spi_id >= SPI_COUNT)
        return 0;
    __builtin_store(spi_data_addr[spi_id], data);
    return __builtin_load(spi_data_addr[spi_id]);
}

void
spi_select(int spi_id)
{
    if (spi_id < 0 || spi_id >= SPI_COUNT)
        return;
    __builtin_store(spi_cs_addr[spi_id], 0);
}

void
spi_deselect(int spi_id)
{
    if (spi_id < 0 || spi_id >= SPI_COUNT)
        return;
    __builtin_store(spi_cs_addr[spi_id], 1);
}
