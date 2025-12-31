# Specifications

!!! Note
    This page needs to be updated as I am switching to the Cyclone IV FPGA on the custom PCB.

## CPU

- 32 Bit
- Fully custom instruction set (called B32P3)
- 5 Stage pipeline (classic MIPS-style: IF, ID, EX, MEM, WB)
- 16 Registers (15 GP and R0 is always 0)
- L1i and L1d cache (direct mapped, 128 cache lines of 8 instructions per line)
    - Cache controller with write-back policy and dirty bit tracking, acting as arbiter for external DRAM
- 32 Bit program counter for 4 GiW (or 16 GiB) of addressable memory
- Shared instruction and data memory
- 50 MHz, with some memory related components running at 100 MHz
- Extendable amount of hardware interrupts

## GPU

- 640x480 HDMI output using TMDS
- Two tile based planes of 40x25 visible characters at 8x8 pixels (320x200 pixels). This is mainly for text and sprites
    - Background plane with horizontal scrolling support (64x25 tiles in memory)
    - Window plane
- One bitmap plane
    - Bitmap resolution of 320x240 at 8bits per pixel (r3g3b2 color depth)
- All video memory is dual port, meaning that the CPU can write at any time during the frame rendering process
- Interrupts the CPU after each frame drawn to allow for synchronization

## Memory

- 112 MiW (448 MiB) DDR3 SDRAM available through L1 cache
- 1 KiW (4 KiB) single cycle ROM
- ~ 100 KiB single cycle dual port dual clock VRAM in various data widths
- 128 entries of CPU hardware stack
- SPI Flash and Micro SD Card available in software through SPI hardware interface

## I/O

Memory mapped I/O via Memory Unit that presents the following I/O:

- 1 UART interface (for USB - UART converter)
- 6 SPI interfaces (2x SPI Flash, 2x SPI USB Host, 1x ENC28J60, 1x Micro SD)
- 8 GPIO pins
- 3 One shot timers
- Registers for boot mode, FPGA temp, Microseconds

## Performance

!!! note
    I still need to evaluate performance once more software is up and running

Hopefully comparable if not faster than an Intel 486 (SX as there is no Floating Point support yet)
