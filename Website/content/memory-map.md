---
title: Memory map
weight: 2
reader: programming
---

The 32-bit address space of the FPGC, as the CPU sees it. All addresses are byte addresses.

| Address range | Size | Region | Reached through |
| --- | --- | --- | --- |
| `0x00000000`–`0x03FFFFFF` | 64 MiB | SDRAM | the L1 caches |
| `0x1C000000`–`0x1DFFFFFF` | 32 MiB | I/O devices | the bus |
| `0x1E000000`–`0x1E000FFF` | 4 KiB | ROM | the fetch port for instructions, the bus for data |
| `0x1F000000`–`0x1F000013` | 5 registers | CPU-internal I/O registers | inside the CPU |

Every other address is unmapped. A read from it gives 0, a write to it is ignored, and both set the bad-address flag of the [error register](cpu/isa.md#error-register).

## SDRAM

The main memory, for code and data. The CPU reaches it through its instruction and data caches, see [Caches](cpu/caches.md).

- A program in SDRAM starts at `0x0`. Address `0x4` is where the CPU goes when it takes an interrupt.
- The contents of SDRAM are undefined after a reset.

## I/O devices

The devices, such as the UART, are reached over the bus. Which device answers at which address is defined with the devices, which are still to be built. Only addresses in this range go to the devices, and the CPU never reads them before it's certain that the instruction will complete.

## ROM

The boot ROM, 1,024 words. The CPU starts at `0x1E000000` after a reset.

- It holds code, and data that the code reads.
- Writes to the ROM are illegal: they're ignored, and set the bad-address flag.

## CPU-internal I/O registers

Five registers inside the CPU: `PC_BACKUP`, `SP`, `ERR_STATUS`, `ERR_PC` and `INT_ENABLE`. They're described on the [instruction set](cpu/isa.md#cpu-internal-io-registers) page.

## Rules

- Only SDRAM and the ROM can hold code. A jump to any other address sets the bad-jump-target flag.
- Word accesses must be at a multiple of 4, and halfword accesses at a multiple of 2, in every region.
- The CPU-internal I/O registers only allow word accesses.
