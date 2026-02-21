//
// enc28j60 library implementation.
//

#include "libs/kernel/io/enc28j60.h"
#include "libs/kernel/io/spi.h"

// ---- Internal State ----
static int enc28j60_current_bank = 0;
static int enc28j60_next_pkt_ptr = 0;

// ---- Low-Level SPI Operations ----
static int enc28j60_read_op(int op, int address)
{
    int result;
    spi_select(ENC28J60_SPI_ID);
    spi_transfer(ENC28J60_SPI_ID, op | (address & ENC_ADDR_MASK));
    result = spi_transfer(ENC28J60_SPI_ID, 0x00);
    if (address & ENC_MII_FLAG)
    {
        result = spi_transfer(ENC28J60_SPI_ID, 0x00);
    }
    spi_deselect(ENC28J60_SPI_ID);
    return result & 0xFF;
}

// enc28j60 write op
static void enc28j60_write_op(int op, int address, int data)
{
    spi_select(ENC28J60_SPI_ID);
    spi_transfer(ENC28J60_SPI_ID, op | (address & ENC_ADDR_MASK));
    spi_transfer(ENC28J60_SPI_ID, data);
    spi_deselect(ENC28J60_SPI_ID);
}

// ---- Register Access (with bank switching) ----
static void enc28j60_set_bank(int address)
{
    int new_bank;
    new_bank = address & ENC_BANK_MASK;
    if (new_bank != enc28j60_current_bank)
    {
        enc28j60_write_op(ENC_OP_BFC, ECON1, ECON1_BSEL1 | ECON1_BSEL0);
        enc28j60_write_op(ENC_OP_BFS, ECON1, new_bank >> 5);
        enc28j60_current_bank = new_bank;
    }
}

// enc28j60 read reg
static int enc28j60_read_reg(int address)
{
    enc28j60_set_bank(address);
    return enc28j60_read_op(ENC_OP_RCR, address);
}

// enc28j60 read reg16
static int enc28j60_read_reg16(int address)
{
    int low;
    int high;
    low = enc28j60_read_reg(address);
    high = enc28j60_read_reg(address + 1);
    return low | (high << 8);
}

// enc28j60 write reg
static void enc28j60_write_reg(int address, int data)
{
    enc28j60_set_bank(address);
    enc28j60_write_op(ENC_OP_WCR, address, data);
}

// enc28j60 write reg16
static void enc28j60_write_reg16(int address, int data)
{
    enc28j60_write_reg(address, data & 0xFF);
    enc28j60_write_reg(address + 1, (data >> 8) & 0xFF);
}

// ---- PHY Register Access ----
static int enc28j60_read_phy(int address)
{
    int result;
    enc28j60_write_reg(MIREGADR, address);
    enc28j60_write_reg(MICMD, MICMD_MIIRD);
    while (enc28j60_read_reg(MISTAT) & MISTAT_BUSY)
        ;
    enc28j60_write_reg(MICMD, 0x00);
    result = enc28j60_read_reg(MIRDL);
    result = result | (enc28j60_read_reg(MIRDH) << 8);
    return result;
}

// enc28j60 write phy
static void enc28j60_write_phy(int address, int data)
{
    enc28j60_write_reg(MIREGADR, address);
    enc28j60_write_reg(MIWRL, data & 0xFF);
    enc28j60_write_reg(MIWRH, (data >> 8) & 0xFF);
    while (enc28j60_read_reg(MISTAT) & MISTAT_BUSY)
        ;
}

// ---- Buffer Memory Read/Write ----
static void enc28j60_read_buffer(char* buf, int len)
{
    int i;
    spi_select(ENC28J60_SPI_ID);
    spi_transfer(ENC28J60_SPI_ID, ENC_OP_RBM);
    i = 0;
    while (i < len)
    {
        buf[i] = spi_transfer(ENC28J60_SPI_ID, 0x00);
        i = i + 1;
    }
    spi_deselect(ENC28J60_SPI_ID);
}

// enc28j60 write buffer
static void enc28j60_write_buffer(char* buf, int len)
{
    int i;
    spi_select(ENC28J60_SPI_ID);
    spi_transfer(ENC28J60_SPI_ID, ENC_OP_WBM);
    i = 0;
    while (i < len)
    {
        spi_transfer(ENC28J60_SPI_ID, buf[i]);
        i = i + 1;
    }
    spi_deselect(ENC28J60_SPI_ID);
}

// ---- RX Buffer Management ----
static void enc28j60_free_rx_space()
{
    // Errata B7 #14: ERXRDPT must be odd
    if (enc28j60_next_pkt_ptr == 0)
    {
        enc28j60_write_reg16(ERXRDPTL, ENC_RXSTOP);
    }
    else
    {
        enc28j60_write_reg16(ERXRDPTL, enc28j60_next_pkt_ptr - 1);
    }
    enc28j60_write_op(ENC_OP_BFS, ECON2, ECON2_PKTDEC);
}

// ---- Public API Implementation ----
int enc28j60_init(int* mac)
{
    int rev;

    spi_deselect(ENC28J60_SPI_ID);

    // Soft reset
    spi_select(ENC28J60_SPI_ID);
    spi_transfer(ENC28J60_SPI_ID, ENC_OP_SRC);
    spi_deselect(ENC28J60_SPI_ID);

    // Errata B7 #2: wait at least 1ms after reset
    delay(2);

    // Wait for CLKRDY
    while (!(enc28j60_read_op(ENC_OP_RCR, ESTAT) & ESTAT_CLKRDY));

    enc28j60_next_pkt_ptr = ENC_RXSTART;

    // RX buffer
    enc28j60_write_reg16(ERXSTL, ENC_RXSTART);
    enc28j60_write_reg16(ERXRDPTL, ENC_RXSTART);
    enc28j60_write_reg16(ERXNDL, ENC_RXSTOP);

    // TX buffer
    enc28j60_write_reg16(ETXSTL, ENC_TXSTART);
    enc28j60_write_reg16(ETXNDL, ENC_TXSTOP);

    // Receive filter: unicast + CRC only.
    // Only accept frames addressed to our MAC (unicast) with valid CRC.
    // Broadcast/multicast frames are rejected in hardware.
    // TODO: reconsider if I want to implement FPGC discovery.
    enc28j60_write_reg(ERXFCON, ERXFCON_UCEN | ERXFCON_CRCEN);

    // MAC init: MARXEN|TXPAUS|RXPAUS
    enc28j60_write_reg(MACON1, MACON1_MARXEN | MACON1_TXPAUS | MACON1_RXPAUS);

    // MACON3: BFS PADCFG0|TXCRCEN|FRMLNEN
    enc28j60_write_op(ENC_OP_BFS, MACON3,
                      MACON3_PADCFG0 | MACON3_TXCRCEN | MACON3_FRMLNEN);

    // Inter-packet gap
    enc28j60_write_reg16(MAIPGL, 0x0C12);

    // Back-to-back inter-packet gap
    enc28j60_write_reg(MABBIPG, 0x12);

    // Max frame length
    enc28j60_write_reg16(MAMXFLL, ENC28J60_MAX_FRAME);

    // Set MAC address
    enc28j60_write_reg(MAADR5, mac[0]);
    enc28j60_write_reg(MAADR4, mac[1]);
    enc28j60_write_reg(MAADR3, mac[2]);
    enc28j60_write_reg(MAADR2, mac[3]);
    enc28j60_write_reg(MAADR1, mac[4]);
    enc28j60_write_reg(MAADR0, mac[5]);

    // PHY: PHCON2
    enc28j60_write_phy(PHCON2, PHCON2_HDLDIS);

    // PHY: PHLCON
    enc28j60_write_phy(PHLCON, 0x0476);

    // ECON2: BFS AUTOINC
    enc28j60_write_op(ENC_OP_BFS, ECON2, ECON2_AUTOINC);

    // EIE: BFS INTIE|PKTIE
    enc28j60_write_op(ENC_OP_BFS, EIE, EIE_INTIE | EIE_PKTIE);

    // ECON1: BFS RXEN
    enc28j60_write_op(ENC_OP_BFS, ECON1, ECON1_RXEN);

    enc28j60_current_bank = 0;

    // Read revision
    rev = enc28j60_read_reg(EREVID);
    if (rev > 5)
    {
        rev = rev + 1;
    }
    return rev;
}

// enc28j60 link up
int enc28j60_link_up()
{
    int phstat2;
    phstat2 = enc28j60_read_phy(PHSTAT2);
    return (phstat2 & PHSTAT2_LSTAT) ? 1 : 0;
}

// enc28j60 get revision
int enc28j60_get_revision()
{
    int rev;
    rev = enc28j60_read_reg(EREVID);
    if (rev > 5)
    {
        rev = rev + 1;
    }
    return rev;
}

// enc28j60 packet count
int enc28j60_packet_count()
{
    return enc28j60_read_reg(EPKTCNT);
}

// enc28j60 packet send
int enc28j60_packet_send(char* buf, int len)
{
    int count;

    if (len <= 0 || len > ENC28J60_MAX_FRAME)
    {
        return 0;
    }

    // Errata #12: reset TX logic
    enc28j60_write_op(ENC_OP_BFS, ECON1, ECON1_TXRST);
    enc28j60_write_op(ENC_OP_BFC, ECON1, ECON1_TXRST);
    enc28j60_write_op(ENC_OP_BFC, EIR, EIR_TXIF | EIR_TXERIF);

    // Set write pointer to TX start
    enc28j60_write_reg16(EWRPTL, ENC_TXSTART);

    // Set TX end
    enc28j60_write_reg16(ETXNDL, ENC_TXSTART + len);

    // Per-packet control byte
    enc28j60_write_op(ENC_OP_WBM, 0, 0x00);

    // Write frame data
    enc28j60_write_buffer(buf, len);

    // Start TX
    enc28j60_write_op(ENC_OP_BFS, ECON1, ECON1_TXRTS);

    // Wait for TXIF or TXERIF
    count = 0;
    while (count < 10000)
    {
        int eir;
        eir = enc28j60_read_op(ENC_OP_RCR, EIR);
        if (eir & (EIR_TXIF | EIR_TXERIF))
        {
            break;
        }
        count = count + 1;
    }

    // Check for error
    if (enc28j60_read_op(ENC_OP_RCR, EIR) & EIR_TXERIF)
    {
        enc28j60_write_op(ENC_OP_BFC, ECON1, ECON1_TXRTS);
        return 0;
    }

    if (count >= 10000)
    {
        enc28j60_write_op(ENC_OP_BFC, ECON1, ECON1_TXRTS);
        return 0;
    }

    return 1;
}

// enc28j60 packet receive
int enc28j60_packet_receive(char* buf, int max_len)
{
    int pkt_count;
    int next_ptr;
    int pkt_len;
    int status;
    char hdr[6];

    pkt_count = enc28j60_read_reg(EPKTCNT);
    if (pkt_count == 0)
    {
        return 0;
    }

    // Set read pointer
    enc28j60_write_reg16(ERDPTL, enc28j60_next_pkt_ptr);

    // Read 6-byte receive status vector
    enc28j60_read_buffer(hdr, 6);

    next_ptr = (hdr[0] & 0xFF) | ((hdr[1] & 0xFF) << 8);
    pkt_len = (hdr[2] & 0xFF) | ((hdr[3] & 0xFF) << 8);
    status = (hdr[4] & 0xFF) | ((hdr[5] & 0xFF) << 8);

    pkt_len = pkt_len - 4; // Remove CRC

    enc28j60_next_pkt_ptr = next_ptr;

    // Check "Received OK" bit (status bit 7)
    if (!(status & 0x80))
    {
        enc28j60_free_rx_space();
        return 0;
    }

    if (pkt_len > max_len)
    {
        pkt_len = max_len;
    }
    if (pkt_len > ENC28J60_MAX_FRAME)
    {
        pkt_len = ENC28J60_MAX_FRAME;
    }

    if (pkt_len > 0)
    {
        enc28j60_read_buffer(buf, pkt_len);
    }

    enc28j60_free_rx_space();

    return pkt_len;
}

// enc28j60 enable broadcast
void enc28j60_enable_broadcast()
{
    int val;
    val = enc28j60_read_reg(ERXFCON);
    enc28j60_write_reg(ERXFCON, val | ERXFCON_BCEN);
}

// enc28j60 disable broadcast
void enc28j60_disable_broadcast()
{
    int val;
    val = enc28j60_read_reg(ERXFCON);
    enc28j60_write_reg(ERXFCON, val & ~ERXFCON_BCEN);
}
