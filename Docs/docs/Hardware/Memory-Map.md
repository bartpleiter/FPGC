# Memory Map

The FPGC maps all CPU-accessible memory and I/O into a flat address space. The map is designed with a few goals: addresses are discontinuous to simplify hardware comparisons (no wide AND gates), SDRAM starts at address 0 to make software compilation easier, and everything fits within 27 bits (max address `0x7FFFFFF`), matching the 27-bit jump constant in the ISA.

## CPU Memory Map

Only SDRAM and ROM can be used as instruction memory. All other regions are data-only.

### SDRAM (Main Memory)

| Address | End | Description |
|---|---|---|
| `0x0000000` | `0x6FFFFFF` | Up to 112 MiW of main memory |

Accessed through L1I and L1D caches. Most reads complete in 1 cycle on a cache hit. See [CPU Memory System](CPU/Memory.md) for cache details.

### I/O Peripherals

| Address | Peripheral | Access |
|---|---|---|
| `0x7000000` | UART TX (USB) | Write |
| `0x7000001` | UART RX (USB) | Read |
| `0x7000002` | Timer 1 value | Write |
| `0x7000003` | Timer 1 trigger | Write |
| `0x7000004` | Timer 2 value | Write |
| `0x7000005` | Timer 2 trigger | Write |
| `0x7000006` | Timer 3 value | Write |
| `0x7000007` | Timer 3 trigger | Write |
| `0x7000008` | SPI0 data (Flash 1) | R/W |
| `0x7000009` | SPI0 chip select | R/W |
| `0x700000A` | SPI1 data (Flash 2) | R/W |
| `0x700000B` | SPI1 chip select | R/W |
| `0x700000C` | SPI2 data (USB Host 1) | R/W |
| `0x700000D` | SPI2 chip select | R/W |
| `0x700000E` | SPI2 interrupt pin | Read |
| `0x700000F` | SPI3 data (USB Host 2) | R/W |
| `0x7000010` | SPI3 chip select | R/W |
| `0x7000011` | SPI3 interrupt pin | Read |
| `0x7000012` | SPI4 data (Ethernet) | R/W |
| `0x7000013` | SPI4 chip select | R/W |
| `0x7000014` | SPI4 interrupt pin | Read |
| `0x7000015` | SPI5 data (SD Card) | R/W |
| `0x7000016` | SPI5 chip select | R/W |
| `0x7000017` | GPIO mode | Not yet implemented |
| `0x7000018` | GPIO state | Not yet implemented |
| `0x7000019` | Boot mode | Read |
| `0x700001A` | Microsecond counter | Read |
| `0x700001B` | User LED | Write |

All I/O accesses go through the [Memory Unit](Memory-Unit.md), which stalls the CPU pipeline until complete.

### Single-Cycle Memory

These are implemented in on-chip block RAM (BRAM) or external SRAM and are accessed directly by the CPU without caching.

| Address | End | Region | Description |
|---|---|---|---|
| `0x7800000` | `0x78003FF` | ROM | 1 KiW boot ROM. Initial PC value. |
| `0x7900000` | `0x790041F` | VRAM32 | Tile patterns and palette table |
| `0x7A00000` | `0x7A02001` | VRAM8 | Tile maps, color tables, scroll parameters |
| `0x7B00000` | `0x7B12BFF` | VRAMpixel | 320x240 pixel framebuffer (external SRAM) |

See [GPU](GPU.md) for details on the VRAM contents and how the GPU reads them.

### CPU Internal I/O

These registers are handled inside the CPU core itself, not through the Memory Unit.

| Address | Name | R/W | Description |
|---|---|---|---|
| `0x7C00000` | PC Backup | R/W | Saved program counter from last interrupt. Read to get the resume address; write to redirect execution on `reti`. |
| `0x7C00001` | HW Stack Pointer | R/W | 8-bit hardware stack pointer (0-255). Read to check stack depth; write to restore or reset it. |

## GPU Memory Map

The GPU has its own view of the VRAM memories through the second port of each dual-port RAM. These addresses are internal to the GPU and not accessible from the CPU.

### VRAM32 (GPU side)

| Local Address | Content |
|---|---|
| `0x000` - `0x3FF` | Pattern Table (1024 tile patterns) |
| `0x400` - `0x41F` | Palette Table (8 palettes of 4 colors) |

### VRAM8 (GPU side)

| Local Address | Content |
|---|---|
| `0x000` - `0x7FF` | BG Tile Table (64x32 grid) |
| `0x800` - `0xFFF` | BG Color Table |
| `0x1000` - `0x17FF` | Window Tile Table (40x25 grid) |
| `0x1800` - `0x1FFF` | Window Color Table |
| `0x2000` - `0x2001` | Scroll parameters (BG tile offset, fine pixel offset) |

### VRAMpixel (GPU side)

| Local Address | Content |
|---|---|
| `0x000` - `0x12BFF` | 320x240 8-bit pixel values (76,800 bytes) |
