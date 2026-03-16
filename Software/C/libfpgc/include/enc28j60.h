/*
 * enc28j60.h — ENC28J60 Ethernet Controller driver for B32P3/FPGC.
 *
 * Provides low-level Ethernet frame send/receive for the Microchip ENC28J60.
 * Communicates via SPI.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef FPGC_ENC28J60_H
#define FPGC_ENC28J60_H

/* SPI ID to use */
#define ENC28J60_SPI_ID SPI_ETH

/* Maximum Ethernet frame size (without CRC) */
#define ENC28J60_MAX_FRAME 1518

/* ---- SPI Opcodes ---- */
#define ENC_OP_RCR 0x00  /* Read Control Register */
#define ENC_OP_RBM 0x3A  /* Read Buffer Memory */
#define ENC_OP_WCR 0x40  /* Write Control Register */
#define ENC_OP_WBM 0x7A  /* Write Buffer Memory */
#define ENC_OP_BFS 0x80  /* Bit Field Set */
#define ENC_OP_BFC 0xA0  /* Bit Field Clear */
#define ENC_OP_SRC 0xFF  /* System Reset Command */

/* ---- Register address encoding helpers ---- */
#define ENC_ADDR_MASK 0x1F
#define ENC_BANK_MASK 0x60
#define ENC_MII_FLAG  0x80

/* ---- Bank 0 registers ---- */
#define ERDPTL   0x00
#define ERDPTH   0x01
#define EWRPTL   0x02
#define EWRPTH   0x03
#define ETXSTL   0x04
#define ETXSTH   0x05
#define ETXNDL   0x06
#define ETXNDH   0x07
#define ERXSTL   0x08
#define ERXSTH   0x09
#define ERXNDL   0x0A
#define ERXNDH   0x0B
#define ERXRDPTL 0x0C
#define ERXRDPTH 0x0D

/* ---- Bank 1 registers ---- */
#define ERXFCON 0x38
#define EPKTCNT 0x39
#define EPMM0   0x28
#define EPMM1   0x29
#define EPMCSL  0x30
#define EPMCSH  0x31
#define EPMOL   0x34
#define EPMOH   0x35

/* ---- Bank 2 registers (MAC/MII) ---- */
#define MACON1   0xC0
#define MACON3   0xC2
#define MABBIPG  0xC4
#define MAIPGL   0xC6
#define MAIPGH   0xC7
#define MAMXFLL  0xCA
#define MAMXFLH  0xCB
#define MICMD    0xD2
#define MIREGADR 0xD4
#define MIWRL    0xD6
#define MIWRH    0xD7
#define MIRDL    0xD8
#define MIRDH    0xD9

/* ---- Bank 3 registers ---- */
#define MAADR1 0xE0
#define MAADR0 0xE1
#define MAADR3 0xE2
#define MAADR2 0xE3
#define MAADR5 0xE4
#define MAADR4 0xE5
#define EREVID 0x72
#define MISTAT 0xEA

/* ---- Shared registers (all banks) ---- */
#define EIE   0x1B
#define EIR   0x1C
#define ESTAT 0x1D
#define ECON2 0x1E
#define ECON1 0x1F

/* ---- Bit masks ---- */
#define ECON1_BSEL0 0x01
#define ECON1_BSEL1 0x02
#define ECON1_RXEN  0x04
#define ECON1_TXRTS 0x08
#define ECON1_TXRST 0x80

#define ECON2_AUTOINC 0x80
#define ECON2_PKTDEC  0x40

#define ESTAT_CLKRDY 0x01

#define EIE_PKTIE 0x40
#define EIE_INTIE 0x80

#define EIR_PKTIF  0x40
#define EIR_TXERIF 0x02
#define EIR_TXIF   0x08

#define ERXFCON_BCEN  0x01
#define ERXFCON_PMEN  0x10
#define ERXFCON_CRCEN 0x20
#define ERXFCON_UCEN  0x80

#define MACON1_MARXEN 0x01
#define MACON1_RXPAUS 0x04
#define MACON1_TXPAUS 0x08

#define MACON3_FRMLNEN 0x02
#define MACON3_TXCRCEN 0x10
#define MACON3_PADCFG0 0x20

#define MICMD_MIIRD 0x01
#define MISTAT_BUSY 0x01

/* ---- PHY Registers ---- */
#define PHCON2  0x10
#define PHSTAT2 0x11
#define PHLCON  0x14

#define PHCON2_HDLDIS  0x0100
#define PHSTAT2_LSTAT  0x0400

/* ---- Buffer layout ---- */
#define ENC_RXSTART 0x0000
#define ENC_RXSTOP  0x19FF
#define ENC_TXSTART 0x1A00
#define ENC_TXSTOP  0x1FFF

/* ---- Public API ---- */
int  enc28j60_init(int *mac);
int  enc28j60_link_up(void);
int  enc28j60_get_revision(void);
int  enc28j60_packet_count(void);
int  enc28j60_packet_send(char *buf, int len);
int  enc28j60_packet_receive(char *buf, int max_len);
void enc28j60_enable_broadcast(void);
void enc28j60_disable_broadcast(void);

/* ---- ISR support ---- */
extern int enc28j60_spi_in_use;
void enc28j60_isr_begin(void);
void enc28j60_isr_end(void);

#endif /* FPGC_ENC28J60_H */
