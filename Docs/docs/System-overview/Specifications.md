# Specifications

A quick-reference summary of the FPGC's hardware and software specifications.

## CPU (B32P3)

| Spec | Value |
|---|---|
| Architecture | 32-bit RISC, custom ISA |
| Pipeline | Classic 5-stage (IF, ID, EX, MEM, WB) |
| Clock | 100 MHz |
| Registers | 16 (r0 hardwired to zero) |
| Hardware stack | 256 entries |
| Instruction width | 32 bits, 16 opcodes |
| Address space | 32-bit byte-addressable |
| ALU | Add, sub, logic, shift, multiply, divide, fixed-point multiply/divide |
| L1I cache | 128 lines × 8 words, direct-mapped, next-line prefetch |
| L1D cache | 128 lines × 8 words, direct-mapped, write-back with dirty bit |
| Interrupts | 8 hardware interrupts, vectored through address 4 |

## GPU (FSX)

| Spec | Value |
|---|---|
| Output | 640×480, 60 Hz, HDMI (TMDS) |
| Render resolution | 320×240, 2× scaled |
| BGW plane | Two layers of 8×8 tiles, 40×25 visible grid, 256 tile patterns, 256 palettes of 4 colors |
| Pixel plane | 320×240 bitmap, 8-bit color (R3G3B2), external SRAM |
| VRAM | ~100 KiB dual-port block RAM, CPU-writable at any time |
| Sync | Frame-drawn interrupt at vblank |

## Memory

| Component | Type | Size | Access |
|---|---|---|---|
| SDRAM | 2× W9825G6KH-6 | 64 MiB | Via L1 caches, 256-bit burst |
| ROM | FPGA block RAM | 4 KiB (1 KiW) | Single-cycle |
| VRAM | FPGA block RAM | ~100 KiB | Dual-port, GPU + CPU |
| SRAM | IS61LV5128AL | 512 KB | Pixel framebuffer |
| SPI Flash | 2× W25Q128 | 32 MiB total | FPGA config + filesystem |
| SD Card | External, via SPI | Variable | Mass storage |

## I/O

| Peripheral | Count | Notes |
|---|---|---|
| UART | 1 | USB via CH340C, 1 Mbaud |
| SPI | 6 | 2× Flash, 2× USB Host (CH376T), 1× Ethernet (ENC28J60), 1× SD Card |
| Timer | 3 | One-shot, interrupt on trigger |
| GPIO | 8 pins | General-purpose |

## FPGA

| Spec | Value |
|---|---|
| Device | Cyclone IV EP4CE40F23I7N |
| Logic elements | 39,600 |
| Block RAM | 1.1 Mbit |
| Multipliers | 116 |
| Clocks | 100 MHz (CPU), 25 MHz (GPU), 125 MHz (TMDS), 100 MHz 180° (SDRAM) |

## Software

| Component | Description |
|---|---|
| ASMPY | Python assembler for B32P3 ISA |
| cproc + QBE | C11 compiler toolchain, targets B32P3 assembly |
| BDOS | Custom OS with shell, syscalls, program loading |
| BRFS | FAT-based filesystem, RAM-cached with SPI Flash persistence |
| FNP | Custom Layer 2 Ethernet protocol for file transfer and remote input |
