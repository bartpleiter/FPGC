#ifndef ENC28J60_H
#define ENC28J60_H

// ENC28J60 Ethernet Controller Library
// Provides low-level Ethernet frame send/receive for the Microchip ENC28J60.
// Communicates via SPI.
// Mostly generated code to prevent having to having to spend a lot of time writing a library
// for a chip that is already widely supported by existing libraries.

// SPI ID to use
#define ENC28J60_SPI_ID SPI_ID_ETH

// Maximum Ethernet frame size (without CRC)
#define ENC28J60_MAX_FRAME 1518

// ---- SPI Opcodes ----
#define ENC_OP_RCR 0x00 // Read Control Register
#define ENC_OP_RBM 0x3A // Read Buffer Memory
#define ENC_OP_WCR 0x40 // Write Control Register
#define ENC_OP_WBM 0x7A // Write Buffer Memory
#define ENC_OP_BFS 0x80 // Bit Field Set
#define ENC_OP_BFC 0xA0 // Bit Field Clear
#define ENC_OP_SRC 0xFF // System Reset Command

// ---- Register address encoding helpers ----
// Bits 0-4: register address within bank.
// Bits 5-6: bank number (0-3).
// Bit 7: 1 for MAC/MII registers (requires a dummy SPI read byte).
#define ENC_ADDR_MASK 0x1F
#define ENC_BANK_MASK 0x60
#define ENC_MII_FLAG 0x80

// ---- Bank 0 registers ----
#define ERDPTL 0x00
#define ERDPTH 0x01
#define EWRPTL 0x02
#define EWRPTH 0x03
#define ETXSTL 0x04
#define ETXSTH 0x05
#define ETXNDL 0x06
#define ETXNDH 0x07
#define ERXSTL 0x08
#define ERXSTH 0x09
#define ERXNDL 0x0A
#define ERXNDH 0x0B
#define ERXRDPTL 0x0C
#define ERXRDPTH 0x0D

// ---- Bank 1 registers ----
#define ERXFCON 0x38 // 0x18 | bank1(0x20)
#define EPKTCNT 0x39 // 0x19 | bank1(0x20)
#define EPMM0 0x28   // 0x08 | bank1(0x20) - Pattern match mask byte 0-7
#define EPMM1 0x29   // 0x09 | bank1(0x20) - Pattern match mask byte 8-15
#define EPMCSL 0x30  // 0x10 | bank1(0x20) - Pattern match checksum low
#define EPMCSH 0x31  // 0x11 | bank1(0x20) - Pattern match checksum high
#define EPMOL 0x34   // 0x14 | bank1(0x20) - Pattern match offset low
#define EPMOH 0x35   // 0x15 | bank1(0x20) - Pattern match offset high

// ---- Bank 2 registers (MAC/MII - 0x40 | 0x80 = 0xC0 base) ----
#define MACON1 0xC0   // 0x00 | 0x40 | 0x80
#define MACON3 0xC2   // 0x02 | 0x40 | 0x80
#define MABBIPG 0xC4  // 0x04 | 0x40 | 0x80
#define MAIPGL 0xC6   // 0x06 | 0x40 | 0x80
#define MAIPGH 0xC7   // 0x07 | 0x40 | 0x80
#define MAMXFLL 0xCA  // 0x0A | 0x40 | 0x80
#define MAMXFLH 0xCB  // 0x0B | 0x40 | 0x80
#define MICMD 0xD2    // 0x12 | 0x40 | 0x80
#define MIREGADR 0xD4 // 0x14 | 0x40 | 0x80
#define MIWRL 0xD6    // 0x16 | 0x40 | 0x80
#define MIWRH 0xD7    // 0x17 | 0x40 | 0x80
#define MIRDL 0xD8    // 0x18 | 0x40 | 0x80
#define MIRDH 0xD9    // 0x19 | 0x40 | 0x80

// ---- Bank 3 registers ----
#define MAADR1 0xE0 // 0x00 | 0x60 | 0x80
#define MAADR0 0xE1 // 0x01 | 0x60 | 0x80
#define MAADR3 0xE2 // 0x02 | 0x60 | 0x80
#define MAADR2 0xE3 // 0x03 | 0x60 | 0x80
#define MAADR5 0xE4 // 0x04 | 0x60 | 0x80
#define MAADR4 0xE5 // 0x05 | 0x60 | 0x80
#define EREVID 0x72 // 0x12 | 0x60
#define MISTAT 0xEA // 0x0A | 0x60 | 0x80

// ---- Shared registers (all banks) ----
#define EIE 0x1B
#define EIR 0x1C
#define ESTAT 0x1D
#define ECON2 0x1E
#define ECON1 0x1F

// ---- Bit masks ----
// ECON1
#define ECON1_BSEL0 0x01
#define ECON1_BSEL1 0x02
#define ECON1_RXEN 0x04
#define ECON1_TXRTS 0x08
#define ECON1_TXRST 0x80

// ECON2
#define ECON2_AUTOINC 0x80
#define ECON2_PKTDEC 0x40

// ESTAT
#define ESTAT_CLKRDY 0x01

// EIE
#define EIE_PKTIE 0x40
#define EIE_INTIE 0x80

// EIR
#define EIR_TXERIF 0x02
#define EIR_TXIF 0x08

// ERXFCON
#define ERXFCON_BCEN 0x01
#define ERXFCON_PMEN 0x10
#define ERXFCON_CRCEN 0x20
#define ERXFCON_UCEN 0x80

// MACON1
#define MACON1_MARXEN 0x01
#define MACON1_RXPAUS 0x04
#define MACON1_TXPAUS 0x08

// MACON3
#define MACON3_FRMLNEN 0x02
#define MACON3_TXCRCEN 0x10
#define MACON3_PADCFG0 0x20

// MICMD
#define MICMD_MIIRD 0x01

// MISTAT
#define MISTAT_BUSY 0x01

// ---- PHY Registers ----
#define PHCON2 0x10
#define PHSTAT2 0x11
#define PHLCON 0x14

// PHY bits
#define PHCON2_HDLDIS 0x0100
#define PHSTAT2_LSTAT 0x0400

// ---- Buffer layout ----
#define ENC_RXSTART 0x0000
#define ENC_RXSTOP 0x19FF
#define ENC_TXSTART 0x1A00
#define ENC_TXSTOP 0x1FFF

// ---- External functions ----
// Initialize the ENC28J60 with the given MAC address.
int enc28j60_init(int *mac);

// Check if the Ethernet link is up.
int enc28j60_link_up();

// Get the ENC28J60 hardware revision.
int enc28j60_get_revision();

// Get the number of pending received packets.
int enc28j60_packet_count();

// Send a raw Ethernet frame.
// The buffer must contain a complete Ethernet frame starting with
// the destination MAC address (14 bytes header + payload).
// CRC is appended automatically by the hardware.
int enc28j60_packet_send(char *buf, int len);

// Receive a raw Ethernet frame.
// Copies the next pending packet into the supplied buffer.
int enc28j60_packet_receive(char *buf, int max_len);

// Enable reception of broadcast frames.
void enc28j60_enable_broadcast();

// Disable reception of broadcast frames.
void enc28j60_disable_broadcast();

#endif // ENC28J60_H
