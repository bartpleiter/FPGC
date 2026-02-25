# Architecture

The FPGC is built from three main hardware modules (CPU, GPU, Memory Unit) sharing a common memory bus, plus a software stack that runs on top. This page gives the big-picture view of how everything connects. Each component has its own detailed page elsewhere in the docs.

!!! note
    The picture needs to be updated as it was very outdated

## Hardware

### CPU (B32P3)

The B32P3 is a 32-bit RISC processor with a 5-stage pipeline running at 100 MHz. It has 16 registers, a 256-entry hardware stack, and a word-addressable address space. Instructions are fixed-width 32-bit, with 16 opcodes covering arithmetic, memory access, control flow, and system operations.

The CPU fetches instructions from either ROM (first 1 KiW) or SDRAM (through the L1I cache). Data reads and writes go through the L1D cache for SDRAM, or directly to VRAM, ROM, and I/O peripherals. Both caches are direct-mapped with 128 lines of 8 words each, managed by a write-back cache controller.

See [CPU](../Hardware/CPU/CPU.md) for the full ISA and pipeline details.

### GPU (FSX)

The Frame Synthesizer generates 640×480 HDMI video at 60 Hz from two rendering layers:

- **BGW plane**: a tile renderer with background scrolling and a window overlay, similar to the NES PPU. Uses 8×8 tiles at 2× scale, giving 40×25 visible characters.
- **Pixel plane**: a 320×240 bitmap framebuffer at 8-bit color (R3G3B2), stored in external SRAM.

The GPU runs off its own 25 MHz pixel clock and reads VRAM independently from the CPU. A frame-drawn interrupt lets software synchronize to vblank.

See [GPU](../Hardware/GPU.md) for rendering details and memory layout.

### Memory Unit (MU)

The Memory Unit handles all slow I/O peripherals: UART, six SPI masters, three timers, GPIO, and several configuration registers. It sits between the CPU pipeline and the peripheral controllers, presenting a simple start/done stalling interface.

High-speed paths (SDRAM, ROM, VRAM) bypass the MU entirely and connect directly to the pipeline's MEM stage.

See [Memory Unit](../Hardware/Memory-Unit.md) for peripheral details, and [Memory Map](../Hardware/Memory-Map.md) for the full address layout.

### Memory

| Component | Size | Purpose |
|---|---|---|
| SDRAM | 64 MiB (2× W9825G6KH-6) | Main memory for code and data |
| ROM | 1 KiW (4 KiB) | Bootloader |
| VRAM | ~100 KiB (FPGA block RAM) | Tile maps, palettes, pattern tables |
| SRAM | 512 KB (IS61LV5128AL) | Pixel framebuffer |
| SPI Flash | 2× 16 MiB (W25Q128) | Persistent storage (FPGA config + filesystem) |
| SD Card | External | Mass storage via SPI |

## Software

### Bootloader

On power-up, the CPU starts executing from ROM. The ROM bootloader displays a logo, checks the boot DIP switch, and either copies code from SPI Flash into SDRAM or hands off to a UART bootloader for development. See [Bootloaders](../Software/Bootloaders.md).

### OS (BDOS)

BDOS is the operating system, loaded from SPI Flash. It provides a shell, filesystem access (BRFS), hardware drivers, and syscalls for user programs. User programs are loaded into dedicated memory slots and run position-independent code. See [OS](../Software/OS.md).

### Toolchain

- **ASMPY**: Python-based assembler for the B32P3 ISA. See [Assembler](../Software/Assembler.md).
- **B32CC**: C compiler derived from SmallerC, targeting B32P3 assembly. See [C Compiler](../Software/C-compiler.md).
- **BRFS**: Custom FAT-based filesystem cached in RAM with SPI Flash backing. See [BRFS](../Software/BRFS.md).
- **FNP**: Custom Layer 2 Ethernet protocol for file transfers and remote keyboard input. See [FNP](../Software/FNP.md).

## Data Flow

A typical instruction execution goes through these paths:

1. **Fetch**: PC → L1I cache → (on miss) cache controller → SDRAM → fill L1I → deliver instruction
2. **Execute**: ALU operates on register values
3. **Memory access**: Depending on the address:
    - SDRAM range → L1D cache → (on miss) cache controller → SDRAM
    - VRAM range → direct dual-port RAM access (single cycle)
    - I/O range → Memory Unit → peripheral controller → stall until done
4. **Write back**: Result written to register file

The GPU operates independently on its own clock domain, reading VRAM and SRAM concurrently with CPU execution. The only synchronization point is the vblank interrupt.
