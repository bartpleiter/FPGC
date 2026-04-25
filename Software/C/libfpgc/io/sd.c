/*
 * SD card SPI-mode driver -- B.5.1 initial drop: init only.
 *
 * SPI command frame (6 bytes):
 *   [0] 0x40 | cmd_index
 *   [1..4] argument (big-endian)
 *   [5] CRC7 << 1 | 1
 *
 * R1 response: a single byte read back; bit 7 must be 0. After a
 * command we keep clocking dummy bytes (0xFF) until we see a byte
 * with bit 7 = 0; that's R1. Some commands return R3/R7 (R1 + 4
 * bytes) -- caller drains the extra bytes.
 *
 * The 0xFF clock pattern is critical: SD cards use CMD/data lines
 * that are pulled high; clocking 0xFF keeps DI high while supplying
 * SCK to the card.
 *
 * Initialisation per SD Physical Layer Simplified Spec v6.00 §7.2:
 *   1. >= 74 SCK cycles with CS=high, DI=high (we send 10 0xFF bytes).
 *   2. CS=low; CMD0 (GO_IDLE_STATE), expect R1=0x01.
 *   3. CMD8 (SEND_IF_COND, arg=0x000001AA, CRC=0x87): R7. If illegal
 *      command (R1 bit 2 set) the card is SDSC -- we reject.
 *   4. ACMD41 (CMD55 + CMD41 with HCS=1, arg=0x40000000) loop until
 *      R1=0x00 or timeout.
 *   5. CMD58 (READ_OCR): drains 4 OCR bytes; CCS bit (OCR[0] bit 6)
 *      must be 1 (SDHC/SDXC). If 0, reject.
 *
 * The host clock is whatever SPI5 is wired to (currently 25 MHz on a
 * 100 MHz fabric clock per MemoryUnit.v). Many modern microSD cards
 * tolerate a 25 MHz init clock; if a particular card refuses, the
 * cleanest fix is a per-controller divider (out of scope for B.5.1).
 *
 * No CRC checking on responses yet (CRC is required for CMD0/CMD8
 * commands themselves and we hand-encode those; bus CRC for data
 * blocks is added in B.5.2).
 */

#include "sd.h"
#include "spi.h"
#include "fpgc.h"
#include "dma.h"

#define SPI_SD       FPGC_SPI_SD_CARD     /* spi id 5 */
#define SD_DUMMY     0xFF

/* DMA constraints (matches DMAengine.v: 32-byte / line alignment, dst in
 * SDRAM 0..0x04000000). 512 = SD block size is already line-aligned. */
#define SD_DMA_OK(p) ( ((unsigned int)(p) % 32u == 0u) && \
                       ((unsigned int)(p) < 0x04000000u) )

/* Commands */
#define CMD0         0   /* GO_IDLE_STATE */
#define CMD8         8   /* SEND_IF_COND */
#define CMD9         9   /* SEND_CSD */
#define CMD10        10  /* SEND_CID */
#define CMD12        12  /* STOP_TRANSMISSION (multi-block) */
#define CMD17        17  /* READ_SINGLE_BLOCK */
#define CMD18        18  /* READ_MULTIPLE_BLOCK */
#define CMD24        24  /* WRITE_BLOCK */
#define CMD25        25  /* WRITE_MULTIPLE_BLOCK */
#define CMD55        55  /* APP_CMD prefix for ACMD */
#define CMD41        41  /* SEND_OP_COND (ACMD41 with CMD55 prefix) */
#define CMD58        58  /* READ_OCR */

/* Data tokens */
#define DATA_TOKEN_READ_WRITE_SINGLE  0xFE
#define DATA_TOKEN_WRITE_MULTI        0xFC
#define DATA_TOKEN_WRITE_STOP_TRAN    0xFD
#define DATA_RESP_MASK                0x1F
#define DATA_RESP_ACCEPTED            0x05

/* R1 response bits */
#define R1_IDLE_STATE       0x01
#define R1_ERASE_RESET      0x02
#define R1_ILLEGAL_CMD      0x04
#define R1_CRC_ERROR        0x08
#define R1_ERASE_SEQ_ERROR  0x10
#define R1_ADDRESS_ERROR    0x20
#define R1_PARAMETER_ERROR  0x40
/* bit 7 always 0 in a valid R1 */

/* OCR bits (byte 0 of the 4-byte OCR returned by CMD58) */
#define OCR_BYTE0_BUSY      0x80   /* power-up not complete when 0 */
#define OCR_BYTE0_CCS       0x40   /* 1 = SDHC/SDXC, 0 = SDSC */

static int
xfer(int b)
{
    return spi_transfer(SPI_SD, b);
}

static void
clock_dummy_bytes(int n)
{
    int i;
    for (i = 0; i < n; i++)
        (void)xfer(SD_DUMMY);
}

/* Send a 6-byte command frame and return the first byte of the R1
 * response (or 0xFF if no R1 arrived within the timeout). The CRC
 * field is only really validated by the card for CMD0 and CMD8;
 * for other commands we send 0xFF (with the LSB forced to 1) -- the
 * card tolerates this with CRC checking off (default after CMD0). */
static int
send_command(int cmd, unsigned int arg, int crc)
{
    int i;
    int r1;
    /* Dummy byte before the command frame: some cards need a clock
     * cycle to release MISO before they sample DI. */
    (void)xfer(SD_DUMMY);

    (void)xfer(0x40 | (cmd & 0x3F));
    (void)xfer((arg >> 24) & 0xFF);
    (void)xfer((arg >> 16) & 0xFF);
    (void)xfer((arg >> 8) & 0xFF);
    (void)xfer(arg & 0xFF);
    (void)xfer((crc & 0xFE) | 0x01);   /* LSB always 1 */

    /* R1: keep clocking until bit 7 = 0, up to N_CR=8 retries
     * per spec -- be generous. */
    for (i = 0; i < 16; i++) {
        r1 = xfer(SD_DUMMY) & 0xFF;
        if ((r1 & 0x80) == 0)
            return r1;
    }
    return 0xFF;
}

/* Drive the 74+ initial clock pulses with CS deasserted (CS=high,
 * DI=high). 10 0xFF bytes = 80 SCK. */
static void
sd_initial_clocks(void)
{
    spi_deselect(SPI_SD);
    clock_dummy_bytes(10);
}

sd_result_t
sd_init(sd_card_info_t *info_out)
{
    int r1;
    int retries;
    unsigned char ocr[4];
    int i;
    sd_card_info_t info_local;

    /* 1. >= 74 SCK with CS high. */
    sd_initial_clocks();

    /* 2. CMD0 -- GO_IDLE_STATE. CRC = 0x95 (precomputed per spec). */
    spi_select(SPI_SD);
    r1 = send_command(CMD0, 0, 0x95);
    spi_deselect(SPI_SD);
    (void)xfer(SD_DUMMY);                     /* 8-clock gap */
    if (r1 != R1_IDLE_STATE) {
        return SD_ERR_NO_CARD;
    }

    /* 3. CMD8 -- SEND_IF_COND. arg=0x000001AA tells the card we
     *    accept VDD 2.7-3.6 V (0x1) and uses the check-pattern 0xAA.
     *    CRC = 0x87 (precomputed). */
    spi_select(SPI_SD);
    r1 = send_command(CMD8, 0x000001AAu, 0x87);
    if (r1 == 0xFF || (r1 & R1_ILLEGAL_CMD)) {
        spi_deselect(SPI_SD);
        (void)xfer(SD_DUMMY);
        /* Card is legacy SDSC or not present. */
        return SD_ERR_UNSUPPORTED;
    }
    /* CMD8 is R7: drain 4 more bytes (32-bit echo). The lowest 12
     * bits should be {voltage_accepted, check_pattern}. We don't
     * strictly enforce them here; SDHC/SDXC cards always echo. */
    for (i = 0; i < 4; i++)
        (void)xfer(SD_DUMMY);
    spi_deselect(SPI_SD);
    (void)xfer(SD_DUMMY);

    /* 4. ACMD41 loop (HCS=1) until R1=0x00 or timeout. */
    retries = 1000;
    while (retries--) {
        spi_select(SPI_SD);
        r1 = send_command(CMD55, 0, 0xFF);
        spi_deselect(SPI_SD);
        (void)xfer(SD_DUMMY);
        if (r1 == 0xFF) {
            return SD_ERR_TIMEOUT;
        }
        spi_select(SPI_SD);
        r1 = send_command(CMD41, 0x40000000u, 0xFF);
        spi_deselect(SPI_SD);
        (void)xfer(SD_DUMMY);
        if (r1 == 0xFF) {
            return SD_ERR_TIMEOUT;
        }
        if (r1 == 0)
            break;
    }
    if (r1 != 0) {
        return SD_ERR_TIMEOUT;
    }

    /* 5. CMD58 -- READ_OCR. Verify CCS=1 (SDHC/SDXC). */
    spi_select(SPI_SD);
    r1 = send_command(CMD58, 0, 0xFF);
    if (r1 != 0) {
        spi_deselect(SPI_SD);
        (void)xfer(SD_DUMMY);
        return SD_ERR_PROTOCOL;
    }
    for (i = 0; i < 4; i++)
        ocr[i] = xfer(SD_DUMMY) & 0xFF;
    spi_deselect(SPI_SD);
    (void)xfer(SD_DUMMY);

    if ((ocr[0] & OCR_BYTE0_BUSY) == 0) {
        return SD_ERR_TIMEOUT;
    }
    if ((ocr[0] & OCR_BYTE0_CCS) == 0) {
        /* SDSC card -- byte-addressed, rejected for this driver. */
        return SD_ERR_UNSUPPORTED;
    }

    info_local.is_sdhc = 1;
    info_local.blocks  = 0;
    for (i = 0; i < 4; i++)
        info_local.ocr[i] = ocr[i];
    for (i = 0; i < 16; i++) {
        info_local.cid[i] = 0;
        info_local.csd[i] = 0;
    }

    /* 6. CMD9 -- SEND_CSD. CSD v2 (SDHC/SDXC) layout:
     *   csd[0] >> 6 == 1 selects v2.
     *   C_SIZE is bits 69..48 (22 bits), in bytes 7,8,9:
     *      C_SIZE = ((csd[7] & 0x3F)<<16) | (csd[8]<<8) | csd[9]
     *   Capacity in 512-byte blocks = (C_SIZE + 1) * 1024. */
    spi_select(SPI_SD);
    r1 = send_command(CMD9, 0, 0xFF);
    if (r1 == 0) {
        int j;
        int tries;
        int tok;
        tok = 0xFF;
        for (tries = 0; tries < 1024; tries++) {
            tok = xfer(SD_DUMMY) & 0xFF;
            if (tok == DATA_TOKEN_READ_WRITE_SINGLE)
                break;
            if (tok != 0xFF)
                break;   /* error token */
        }
        if (tok == DATA_TOKEN_READ_WRITE_SINGLE) {
            for (j = 0; j < 16; j++)
                info_local.csd[j] = xfer(SD_DUMMY) & 0xFF;
            (void)xfer(SD_DUMMY);   /* CRC[0] */
            (void)xfer(SD_DUMMY);   /* CRC[1] */
            if ((info_local.csd[0] >> 6) == 1) {
                unsigned int c_size;
                c_size = ((unsigned int)(info_local.csd[7] & 0x3F) << 16)
                       | ((unsigned int) info_local.csd[8]         <<  8)
                       | ((unsigned int) info_local.csd[9]);
                info_local.blocks = (c_size + 1u) << 10;
            }
        }
    }
    spi_deselect(SPI_SD);
    (void)xfer(SD_DUMMY);

    if (info_out != 0) {
        *info_out = info_local;
    }
    return SD_OK;
}

/* B.5.2 -- single-block CPU-driven read.
 *
 * Flow per SD Physical Spec v6.00 §7.3.3:
 *   1. CMD17 with arg = lba (SDHC = block addressing).
 *   2. R1 must be 0x00.
 *   3. Poll for the data start token 0xFE (timeout).
 *   4. Read 512 bytes into the user buffer.
 *   5. Read 16-bit CRC (currently discarded -- bus CRC is disabled
 *      by default after CMD0; we keep the protocol bytes flowing).
 */
sd_result_t
sd_read_block(unsigned int lba, void *buf)
{
    unsigned char *p = (unsigned char *)buf;
    int  r1;
    int  tok;
    int  tries;
    int  i;

    if (buf == 0)
        return SD_ERR_PROTOCOL;

    spi_select(SPI_SD);
    r1 = send_command(CMD17, lba, 0xFF);
    if (r1 != 0) {
        spi_deselect(SPI_SD);
        (void)xfer(SD_DUMMY);
        return (r1 == 0xFF) ? SD_ERR_TIMEOUT : SD_ERR_PROTOCOL;
    }

    /* Poll for the data start token. The card may insert any
     * number of 0xFF bytes before the token; an early non-0xFF /
     * non-token byte is an error token (top 4 bits zero, low 4
     * bits encode the error). 1024 tries at 25 MHz is ~330 us. */
    tok = 0xFF;
    for (tries = 0; tries < 4096; tries++) {
        tok = xfer(SD_DUMMY) & 0xFF;
        if (tok == DATA_TOKEN_READ_WRITE_SINGLE)
            break;
        if (tok != 0xFF) {
            spi_deselect(SPI_SD);
            (void)xfer(SD_DUMMY);
            return SD_ERR_PROTOCOL;
        }
    }
    if (tok != DATA_TOKEN_READ_WRITE_SINGLE) {
        spi_deselect(SPI_SD);
        (void)xfer(SD_DUMMY);
        return SD_ERR_TIMEOUT;
    }

    /* DMA fast path: 512 B aligned SDRAM buffer -- one SPI2MEM burst on
     * SPI5 streams the whole payload in one shot. CPU still drives the
     * trailing 16-bit CRC bytes (the engine doesn't know about them). */
    if (SD_DMA_OK(p)) {
        cache_flush_data();
        dma_start_spi(DMA_SPI2MEM, SPI_SD, (unsigned int)p, 0u,
                      (unsigned int)SD_BLOCK_SIZE);
        while (dma_busy())
            ;
        (void)dma_status();
        cache_flush_data();
    } else {
        for (i = 0; i < SD_BLOCK_SIZE; i++)
            p[i] = xfer(SD_DUMMY) & 0xFF;
    }

    (void)xfer(SD_DUMMY);   /* CRC[0] */
    (void)xfer(SD_DUMMY);   /* CRC[1] */

    spi_deselect(SPI_SD);
    (void)xfer(SD_DUMMY);
    return SD_OK;
}

/* B.5.2 -- single-block CPU-driven write.
 *
 * Flow per SD Physical Spec v6.00 §7.3.4:
 *   1. CMD24 with arg = lba.
 *   2. R1 must be 0x00.
 *   3. Send one 0xFF gap byte.
 *   4. Send start token 0xFE, then 512 data bytes, then 16-bit CRC
 *      (filler 0xFFFF -- bus CRC disabled).
 *   5. Read the data response token: ((resp & 0x1F) == 0x05) means
 *      accepted; 0x0B = CRC error; 0x0D = write error.
 *   6. Poll busy: card holds MISO low while programming. We wait
 *      until we read a non-zero byte.
 */
sd_result_t
sd_write_block(unsigned int lba, const void *buf)
{
    const unsigned char *p = (const unsigned char *)buf;
    int  r1;
    int  resp;
    int  busy;
    long tries;
    int  i;

    if (buf == 0)
        return SD_ERR_PROTOCOL;

    spi_select(SPI_SD);
    r1 = send_command(CMD24, lba, 0xFF);
    if (r1 != 0) {
        spi_deselect(SPI_SD);
        (void)xfer(SD_DUMMY);
        return (r1 == 0xFF) ? SD_ERR_TIMEOUT : SD_ERR_PROTOCOL;
    }

    (void)xfer(SD_DUMMY);                          /* one byte gap */
    (void)xfer(DATA_TOKEN_READ_WRITE_SINGLE);      /* start token */
    if (SD_DMA_OK(p)) {
        /* Push any dirty L1d lines back to SDRAM so the engine sees
         * the latest payload bytes. */
        cache_flush_data();
        dma_start_spi(DMA_MEM2SPI, SPI_SD, 0u, (unsigned int)p,
                      (unsigned int)SD_BLOCK_SIZE);
        while (dma_busy())
            ;
        (void)dma_status();
    } else {
        for (i = 0; i < SD_BLOCK_SIZE; i++)
            (void)xfer(p[i]);
    }
    (void)xfer(0xFF);                              /* CRC[0] filler */
    (void)xfer(0xFF);                              /* CRC[1] filler */

    resp = xfer(SD_DUMMY) & 0xFF;
    if ((resp & DATA_RESP_MASK) != DATA_RESP_ACCEPTED) {
        spi_deselect(SPI_SD);
        (void)xfer(SD_DUMMY);
        return SD_ERR_WRITE;
    }

    /* Wait for programming to complete. SD spec allows up to
     * 250 ms typical, 500 ms max for a single-block write.
     * At 25 MHz, ~3 MB/s of dummy bytes; 500 ms ~= 1.5M bytes. */
    busy = 0;
    for (tries = 0; tries < 2000000L; tries++) {
        busy = xfer(SD_DUMMY) & 0xFF;
        if (busy != 0)
            break;
    }
    spi_deselect(SPI_SD);
    (void)xfer(SD_DUMMY);
    if (busy == 0)
        return SD_ERR_TIMEOUT;
    return SD_OK;
}

/* B.5.4 -- multi-block read.
 *
 * v1 implementation: loop over sd_read_block. True CMD18 streaming
 * was attempted but produced count-dependent PROTOCOL errors on the
 * test microSD card (works at <=4 blocks, leaves the card stuck at
 * larger counts -- subsequent CMD17/CMD18 also fail until power-
 * cycle). The CMD17-per-block path is slower (one command frame and
 * one start-token wait per 512 B) but is identical to the proven
 * B.5.3 single-block path and recovers cleanly on any per-block
 * failure. A future iteration can revisit true CMD18 streaming once
 * we understand the failure mode (likely related to mid-stream CMD12
 * timing or per-block start-token timeout vs the card's Nac).
 */
sd_result_t
sd_read_blocks(unsigned int lba, void *buf, unsigned int count)
{
    unsigned char *p = (unsigned char *)buf;
    sd_result_t r;
    unsigned int i;

    if (buf == 0 || count == 0)
        return SD_ERR_PROTOCOL;

    for (i = 0; i < count; i++) {
        r = sd_read_block(lba + i, p);
        if (r != SD_OK)
            return r;
        p += SD_BLOCK_SIZE;
    }
    return SD_OK;
}

/* B.5.4 -- multi-block write. Same v1 strategy as sd_read_blocks:
 * loop over sd_write_block (CMD24-per-block) for reliability. */
sd_result_t
sd_write_blocks(unsigned int lba, const void *buf, unsigned int count)
{
    const unsigned char *p = (const unsigned char *)buf;
    sd_result_t r;
    unsigned int i;

    if (buf == 0 || count == 0)
        return SD_ERR_PROTOCOL;

    for (i = 0; i < count; i++) {
        r = sd_write_block(lba + i, p);
        if (r != SD_OK)
            return r;
        p += SD_BLOCK_SIZE;
    }
    return SD_OK;
}
