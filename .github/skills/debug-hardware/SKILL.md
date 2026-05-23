---
name: debug-hardware
description: 'Debug hardware issues on FPGC: SPI devices, DMA transfers, SD card, Ethernet, USB. Use when asked to debug a hardware problem, add debug output, or diagnose a peripheral issue.'
---
# Debug hardware issues

## UART debug output

The primary debugging tool is UART serial output. Add temporary
debug prints to narrow down issues:

```c
#include "uart.h"

// Print a string
uart_print_str("SD init: CMD0 sent\n");

// Print a hex value
uart_print_hex(register_value);
uart_print_str("\n");
```

**UART uses its own MMIO registers** (`FPGC_UART_TX`, `FPGC_UART_RX`),
not an SPI bus. It's always available and doesn't conflict with other
peripherals.

## Monitor UART output

```
make uart-monitor [uart_port=/dev/ttyUSB0]
```

This opens a terminal showing real-time UART output from the FPGC.

## Common debugging scenarios

### SPI device not responding
1. Check the correct SPI bus is selected (see bus table below)
2. Verify CS (chip select) is being asserted/deasserted correctly
3. Add UART prints before/after SPI transactions
4. Check if another driver is holding the bus (SPI is shared)

### DMA transfer fails
1. Check alignment: source, destination, and count must be 32-byte aligned
2. Check count is in bytes (not words) and > 0
3. Verify the DMA mode matches the transfer direction
4. Check `FPGC_DMA_STATUS` for error flag after transfer
5. If using IRQ mode: verify interrupt handler is registered

### SD card issues
1. Cold boot failures: often timing-related (card needs time to power up)
2. Run `make run-sdcard-init-test` for isolated SD init testing
3. Check ACMD41 response: `0x00` = ready, `0x01` = still initializing
4. SD cards are SDHC only (block-addressed, 512-byte blocks)

### Ethernet issues
1. Check ENC28J60 interrupt flag via `FPGC_ETH_NINT`
2. Verify MAC address is set correctly
3. Use FNP protocol for testing: `make fnp-detect-iface`

### USB keyboard issues
1. CH376 uses INT# pin polling (not SPI interrupt)
2. Timer 1 ISR handles periodic HID report polling (10 ms)
3. Check `FPGC_CH376_0_NINT` / `FPGC_CH376_1_NINT` for interrupt status

## SPI bus quick reference
| Bus | Device | Define |
|-----|--------|--------|
| SPI0 | Flash 0 | `FPGC_SPI_FLASH_0` |
| SPI1 | Flash 1 | `FPGC_SPI_FLASH_1` |
| SPI2 | USB top | `FPGC_SPI_USB_0` |
| SPI3 | USB bottom | `FPGC_SPI_USB_1` |
| SPI4 | Ethernet | `FPGC_SPI_ETH` |
| SPI5 | SD card | `FPGC_SPI_SD_CARD` |

## Bare-metal test programs
For isolating hardware issues, use bare-metal tests (no BDOS):
```
make compile-c-baremetal file=<test>
make run-c-baremetal-uart                # Run via UART programmer
make run-sdcard-init-test                # SD card init
make run-spi1-dma-test                   # DMA via SPI1
```

## Important: clean up debug output
After debugging, **remove all `uart_print_*` calls** before
committing. Debug output in production code can cause SPI bus
contention and timing issues.
