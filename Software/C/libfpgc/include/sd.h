/*
 * SD card SPI-mode driver (libfpgc).
 *
 * Block-oriented API. SDHC / SDXC only (block-addressable, 512-byte
 * logical blocks). SDSC and MMC are explicitly rejected during init
 * to keep the address-translation path single.
 *
 * Phasing follows Docs/plans/dma-followups.md §B.5:
 *   B.5.1 (this file, initial drop): SPI init -- CMD0 / CMD8 /
 *         ACMD41 / CMD58. Returns populated sd_card_info_t.
 *   B.5.2: single-block CPU-driven read/write (CMD17 / CMD24).
 *   B.5.3: DMA integration once SPI5 has burst-mode FIFOs.
 *   B.5.4: multi-block helpers (CMD18 / CMD25).
 *
 * Layered above this, brfs_storage_sdcard_t will translate the BRFS
 * word-addressed API onto these block primitives.
 */

#ifndef FPGC_SD_H
#define FPGC_SD_H

#define SD_BLOCK_SIZE 512   /* SPI mode is fixed at 512 */

typedef enum {
    SD_OK              = 0,
    SD_ERR_NO_CARD     = -1,
    SD_ERR_TIMEOUT     = -2,
    SD_ERR_CRC         = -3,
    SD_ERR_PROTOCOL    = -4,  /* unexpected response */
    SD_ERR_UNSUPPORTED = -5,  /* SDSC / MMC etc.    */
    SD_ERR_WRITE       = -6
} sd_result_t;

typedef struct {
    unsigned int  blocks;     /* card capacity in 512-byte blocks (0 if not yet computed) */
    unsigned char ocr[4];     /* operating conditions register */
    unsigned char cid[16];    /* card identification register */
    unsigned char csd[16];    /* card-specific data */
    unsigned char is_sdhc;    /* always 1 for now (SDSC rejected during init) */
} sd_card_info_t;

/* Bring the card from power-on into a state where CMD17/18/24/25 work.
 * Returns SD_OK on success and fills *info_out (which may be NULL if
 * the caller doesn't need it). */
sd_result_t sd_init(sd_card_info_t *info_out);

/* Single 512-byte block read/write. CPU-driven for now (B.5.2);
 * promoted to DMA in B.5.3 once SPI5 has burst FIFOs. */
sd_result_t sd_read_block (unsigned int lba, void *buf);
sd_result_t sd_write_block(unsigned int lba, const void *buf);

/* Multi-block read/write. CMD18 / CMD25 with explicit STOP_TRAN
 * (CMD12) at the end (B.5.4). Each block is 512 bytes; `count` is
 * the number of blocks. The DMA fast path is used when `buf` is
 * 32-byte aligned and lives in SDRAM. */
sd_result_t sd_read_blocks (unsigned int lba, void *buf, unsigned int count);
sd_result_t sd_write_blocks(unsigned int lba, const void *buf, unsigned int count);

#endif
