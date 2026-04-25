# Memory Map

The FPGC maps all CPU-accessible memory and I/O into a flat byte-addressed space. The map is designed with a few goals: addresses are discontinuous to simplify hardware comparisons (no wide AND gates), SDRAM starts at address 0 to make software compilation easier, and I/O and ROM addresses are placed in the upper region of the 29-bit address space.

!!! note
    The reason why ROM is placed in the upper region of the 29 bit address space while the constant in `jump` is only 27 bits is because for a very long time the CPU was only word-addressable. Since this has been changed to byte-addressable, I had to make to choise to either update the memory map entirely or just keep the ROM at where it is and have the assembler use `jumpo` instead. I chose the latter, but a better solution would have been to move all the single cycle memory addresses into the end of the SDRAM region (which is already way larger than the 64 MiB of data it needs to contain)

## CPU Memory Map

Only SDRAM and ROM can be used as instruction memory. All other regions are data-only.

### SDRAM (Main Memory)

| Address | End | Description |
|---|---|---|
| `0x0000000` | `0x3FFFFFF` | 64 MiB of main memory |

Accessed through L1I and L1D caches. Most reads complete in 1 cycle on a cache hit. See [CPU Memory System](CPU/Memory.md) for cache details.

### I/O Peripherals

| Address | Peripheral | Access |
|---|---|---|
| `0x1C000000` | UART TX (USB) | Write |
| `0x1C000004` | UART RX (USB) | Read |
| `0x1C000008` | Timer 1 value | Write |
| `0x1C00000C` | Timer 1 trigger | Write |
| `0x1C000010` | Timer 2 value | Write |
| `0x1C000014` | Timer 2 trigger | Write |
| `0x1C000018` | Timer 3 value | Write |
| `0x1C00001C` | Timer 3 trigger | Write |
| `0x1C000020` | SPI0 data (Flash 1) | R/W |
| `0x1C000024` | SPI0 chip select | R/W |
| `0x1C000028` | SPI1 data (Flash 2) | R/W |
| `0x1C00002C` | SPI1 chip select | R/W |
| `0x1C000030` | SPI2 data (USB Host 1) | R/W |
| `0x1C000034` | SPI2 chip select | R/W |
| `0x1C000038` | SPI2 interrupt pin | Read |
| `0x1C00003C` | SPI3 data (USB Host 2) | R/W |
| `0x1C000040` | SPI3 chip select | R/W |
| `0x1C000044` | SPI3 interrupt pin | Read |
| `0x1C000048` | SPI4 data (Ethernet) | R/W |
| `0x1C00004C` | SPI4 chip select | R/W |
| `0x1C000050` | SPI4 interrupt pin | Read |
| `0x1C000054` | SPI5 data (SD Card) | R/W |
| `0x1C000058` | SPI5 chip select | R/W |
| `0x1C00005C` | GPIO mode | Not yet implemented |
| `0x1C000060` | GPIO state | Not yet implemented |
| `0x1C000064` | Boot mode | Read |
| `0x1C000068` | Microsecond counter | Read |
| `0x1C00006C` | User LED | Write |
| `0x1C000070` | DMA SRC address | R/W |
| `0x1C000074` | DMA DST address | R/W |
| `0x1C000078` | DMA byte count | R/W |
| `0x1C00007C` | DMA CTRL (mode/start) | R/W |
| `0x1C000080` | DMA STATUS (busy/done/error) | Read |

All I/O accesses go through the [Memory Unit](Memory-Unit.md), which stalls the CPU pipeline until complete.

### Single-Cycle Memory

These are implemented in on-chip block RAM (BRAM) or external SRAM and are accessed directly by the CPU without caching.

| Address | End | Region | Description |
|---|---|---|---|
| `0x1E000000` | `0x1E000FFF` | ROM | 4 KiB (1 KiW) boot ROM. Initial PC value. |
| `0x1E400000` | `0x1E40107C` | VRAM32 | Tile patterns and palette table |
| `0x1E800000` | `0x1E808004` | VRAM8 | Tile maps, color tables, scroll parameters |
| `0x1EC00000` | `0x1EC1FFFF` | VRAMpixel | 320x240 pixel framebuffer (external SRAM, byte-addressable, 128 KiB decode window; only the first 76,800 bytes are visible on the display) |

See [GPU](GPU.md) for details on the VRAM contents and how the GPU reads them.

### CPU Internal I/O

These registers are handled inside the CPU core itself, not through the Memory Unit.

| Address | Name | R/W | Description |
|---|---|---|---|
| `0x1F000000` | PC Backup | R/W | Saved program counter from last interrupt. Read to get the resume address; write to redirect execution on `reti`. |
| `0x1F000004` | HW Stack Pointer | R/W | 8-bit hardware stack pointer (0-255). Read to check stack depth; write to restore or reset it. |

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
