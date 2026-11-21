---
title: Memory map
weight: 2
reader: programming
---

The 32-bit address space of the FPGC, as the CPU sees it. All addresses are byte addresses.

| Address range | Size | Region | Reached through |
| --- | --- | --- | --- |
| `0x00000000`–`0x03FFFFFF` | 64 MiB | SDRAM | the L1 caches |
| `0x04000000`–`0x04000FFF` | 4 KiB | ROM | the fetch port for instructions, the bus for data |
| `0x05000000`–`0x05FFFFFF` | 16 MiB | GPU memories (to be built) | the bus |
| `0x06000000`–`0x06FFFFFF` | 16 MiB | external SRAM (to be built) | the bus |
| `0xFFFF8000`–`0xFFFFFEFF` | 127 × 256 bytes | device registers | the bus |
| `0xFFFFFF00`–`0xFFFFFF13` | 5 registers | CPU-internal I/O registers | inside the CPU |

Every other address is unmapped. A read from it gives 0, a write to it is ignored, and both set the bad-address flag of the [error register](cpu/isa.md#error-register). That includes the addresses inside a region that nothing uses, such as the rest of the ROM's 16 MiB, or the end of a device's 256 bytes.

```text
0x00000000  +--------------------------+
            | SDRAM, 64 MiB            |
0x04000000  +--------------------------+
            | ROM, 4 KiB               |
0x05000000  +--------------------------+
            | GPU memories             |
0x06000000  +--------------------------+
            | external SRAM            |
0x07000000  +--------------------------+
            | unmapped                 |
0xFFFF8000  +--------------------------+
            | device registers         |
0xFFFFFF00  +--------------------------+
            | CPU-internal registers   |
0xFFFFFFFF  +--------------------------+
```

## Why it's laid out this way

- SDRAM starts at 0, so a program starts at `0x0`.
- Every memory is below 128 MiB, the range of the absolute `jump`. So code in SDRAM can jump straight to the ROM, and the other way round.
- The registers are in the last 32 KiB. A read or write adds a sign-extended 16-bit offset to a register, so with r0 as the register, an offset from `-32768` to `-1` reaches `0xFFFF8000` to `0xFFFFFFFF`. Every register is then one instruction away, without loading its address first. The assembler accepts the address itself as the offset:

  ```text
  write 0xFFFF8000 r0 r1      ; the same as write -32768 r0 r1
  ```

- The CPU only tells apart SDRAM, the ROM, its own registers, and the rest. Everything else in the regions above goes over the bus to the device bridge, which knows the devices and memories behind it. A new device then doesn't change the CPU.

## SDRAM

The main memory, for code and data. The CPU reaches it through its instruction and data caches, see [Caches](cpu/caches.md).

- A program in SDRAM starts at `0x0`. Address `0x4` is where the CPU goes when it takes an interrupt.
- The contents of SDRAM are undefined after a reset.

## ROM

The boot ROM, 1,024 words. The CPU starts at `0x04000000` after a reset.

- It holds code, and data that the code reads.
- Writes to the ROM are illegal: they're ignored, and set the bad-address flag.

## GPU memories and external SRAM

These regions are reserved for the GPU, which is still to be designed. Until then, nothing answers there.

## Device registers

The devices are reached over the bus. Each device has 256 bytes, starting at `0xFFFF8000`, and only takes word accesses. The [I/O devices](io/_index.md) pages describe their registers.

| Address | Device |
| --- | --- |
| `0xFFFF8000` | [UART](io/uart.md) |
| `0xFFFF8100` | [cycle counter](io/timer.md#cycle-counter) |
| `0xFFFF8200` | [timer](io/timer.md#timer) |
| `0xFFFF8300` | [board register](io/_index.md#board-register): user LED and DIP switches |
| `0xFFFF8400`–`0xFFFFFEFF` | unused |

The CPU never makes a bus access before it's certain that the instruction will complete. So a device can safely do something when it's read, such as taking a byte from a receive buffer.

## CPU-internal I/O registers

Five registers inside the CPU: `PC_BACKUP`, `SP`, `ERR_STATUS`, `ERR_PC` and `INT_ENABLE`. They're described on the [instruction set](cpu/isa.md#cpu-internal-io-registers) page.

## Rules

- Only SDRAM and the ROM can hold code. A jump to any other address sets the bad-jump-target flag.
- Word accesses must be at a multiple of 4, and halfword accesses at a multiple of 2, in every region.
- The CPU-internal I/O registers only allow word accesses.
