/*
 * spi.c — SPI bus driver implementation for B32P3/FPGC.
 *
 * All 6 SPI buses use memory-mapped data and chip-select registers.
 * I/O goes through hwio_write/hwio_read (from hwio.asm).
 *
 * SPDX-License-Identifier: BSD-3-Clause
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
    hwio_write(spi_data_addr[spi_id], data);
    return hwio_read(spi_data_addr[spi_id]);
}

void
spi_select(int spi_id)
{
    if (spi_id < 0 || spi_id >= SPI_COUNT)
        return;
    hwio_write(spi_cs_addr[spi_id], 0);
}

void
spi_deselect(int spi_id)
{
    if (spi_id < 0 || spi_id >= SPI_COUNT)
        return;
    hwio_write(spi_cs_addr[spi_id], 1);
}
