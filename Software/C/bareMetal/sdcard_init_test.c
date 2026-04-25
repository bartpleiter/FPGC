/*
 * SD card SPI init bring-up test.
 *
 * Phase B.5.1: drives sd_init() over SPI5 and prints the OCR result.
 * No filesystem, no block reads -- just verifies CMD0/CMD8/ACMD41/CMD58
 * succeed against whatever microSD card is in the slot and that the
 * card reports SDHC.
 *
 * Build: `make compile-sdcard-init-test` (target added in top-level
 * Makefile alongside compile-spi1-qspi-test).
 */

#include "uart.h"
#include "sd.h"

static const char *
sd_err_name(sd_result_t r)
{
    switch (r) {
    case SD_OK:              return "OK";
    case SD_ERR_NO_CARD:     return "NO_CARD";
    case SD_ERR_TIMEOUT:     return "TIMEOUT";
    case SD_ERR_CRC:         return "CRC";
    case SD_ERR_PROTOCOL:    return "PROTOCOL";
    case SD_ERR_UNSUPPORTED: return "UNSUPPORTED";
    case SD_ERR_WRITE:       return "WRITE";
    default:                 return "?";
    }
}

int main(void)
{
    sd_card_info_t info;
    sd_result_t    r;
    int i;

    uart_puts("\n=== SD card SPI init bring-up ===\n");
    uart_puts("Calling sd_init()...\n");
    r = sd_init(&info);

    uart_puts("Result: ");
    uart_puts(sd_err_name(r));
    uart_puts("\n");

    if (r != SD_OK) {
        uart_puts("Init failed. Check card is inserted, SDHC/SDXC, and bus wiring.\n");
        while (1) ;
        return 0;
    }

    uart_puts("OCR: ");
    for (i = 0; i < 4; i++) {
        uart_puthex(info.ocr[i], 1);
        uart_puts(" ");
    }
    uart_puts("\n");

    uart_puts("is_sdhc=");
    uart_putint(info.is_sdhc);
    uart_puts(" CCS bit (OCR[0] & 0x40) -> ");
    uart_puthex(info.ocr[0] & 0x40, 1);
    uart_puts("\n");

    uart_puts("=== Done ===\n");
    while (1) ;
    return 0;
}

void interrupt(void)
{
}
