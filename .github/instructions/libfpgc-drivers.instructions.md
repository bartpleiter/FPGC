---
name: 'libfpgc Drivers'
description: 'Rules for editing hardware drivers and low-level libraries'
applyTo: 'Software/C/libfpgc/**'
---
# libfpgc driver guidelines

## Validation
After any change: `make compile-bdos`
For filesystem changes: also `make test-host`

## File map — I/O drivers (`libfpgc/io/`)
| File | Device | SPI Bus |
|------|--------|---------|
| `uart.c` | UART serial | — (MMIO direct) |
| `spi.c` | SPI bus abstraction | all (0–5) |
| `spi_flash.c` | SPI NOR flash (BRFS root) | SPI0 or SPI1 |
| `sd.c` | SD card (SDHC/SDXC) | SPI5 |
| `ch376.c` | USB host controller | SPI2 (top), SPI3 (bottom) |
| `enc28j60.c` | Ethernet controller | SPI4 |
| `dma.c` | DMA engine (7 modes) | — (MMIO) |
| `timer.c` | Hardware timers (0–2) | — (MMIO) |

## File map — Filesystem (`libfpgc/fs/`)
| File | Purpose |
|------|---------|
| `brfs.c` | BRFS filesystem core (format, open, read, write, mkdir, readdir) |
| `brfs_cache.c` | LRU block cache |
| `brfs_storage_spi_flash.c` | SPI flash storage backend (vtable) |
| `brfs_storage_sdcard.c` | SD card storage backend (vtable) |

## SPI bus assignments
| Bus | Device | Defines |
|-----|--------|---------|
| SPI0 | Flash chip 0 | `FPGC_SPI_FLASH_0` |
| SPI1 | Flash chip 1 | `FPGC_SPI_FLASH_1` |
| SPI2 | CH376 USB top | `FPGC_SPI_USB_0` |
| SPI3 | CH376 USB bottom | `FPGC_SPI_USB_1` |
| SPI4 | ENC28J60 Ethernet | `FPGC_SPI_ETH` |
| SPI5 | SD card | `FPGC_SPI_SD_CARD` |

## MMIO access
Always use builtins — never `volatile`:
```c
int val = __builtin_load((int*)FPGC_TIMER0_VAL);
__builtin_store((int*)FPGC_UART_TX, ch);
```

## DMA usage
```c
#include "dma.h"
// Modes: MEM2MEM(0), MEM2SPI(1), SPI2MEM(2), MEM2VRAM(3),
//        MEM2IO(4), IO2MEM(5), SPI2MEM_QSPI(6)
// DMA transfers must be word-aligned (4-byte boundary)
// Count is in words (4 bytes each)
// Use dma_transfer_blocking() or enable IRQ with FPGC_DMA_CTRL_IRQ_EN
```

## Ripple effects
- Changing a driver's public API → update the header in `include/` AND all callers in `bdos/`
- Adding a new storage backend → implement the `brfs_storage_ops` vtable, register in `fs.c`
- Changing SPI bus usage → update `fpgc.h` defines, Verilog SPI module assignments

## Reference implementations
- New SPI device driver → study `sd.c` (cleanest example: init, read, write, error handling)
- New storage backend → study `brfs_storage_sdcard.c` (vtable pattern)
