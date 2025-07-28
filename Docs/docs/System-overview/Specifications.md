# Specifications

!!! note
    Specifications are still mostly WIP until hardware design has finalized, which will be when a PCB design has been created

## CPU (WIP)

- 32 Bit
- Fully custom instruction set (called B32P2)
- 6 Stage pipeline
- 16 Registers (15 GP and R0 is always 0)
- L1i and L1d cache (direct mapped, 128 cache lines of 8 instructions per line)
- 32 Bit program counter for 4 GiW (or 16 GiB) of addressable instruction memory
- Shared instruction and data memory
- 50 MHz, with some memory related components running at 100 MHz
- Extendable amount of hardware interrupts

## GPU

- 480P HDMI output using TMDS
- Two tile based planes of 40x25 visible characters at 8x8 pixels (320x200 pixels). This is mainly for text and sprites
    - Background plane with horizontal scrolling support (64x25 tiles in memory)
    - Window plane
- One bitmap plane
    - Bitmap resolution of 320x240 at 8bits per pixel (r3g3b2 color depth)
- All video memory is dual port, meaning that the CPU can write at any time during the frame (although for the bitmap plane you can get tearing)
- Interrupts the CPU after each frame drawn


## Memory (WIP)

!!! note
    This section does not include video memory or cache, just the external memory the device can interface with

These amounts are mainly chosen to fit within a 27 bit address, as the static jump instruction can directly set the PC to any 27 bit address. If needed in the future, much more could be enabled by using the JUMPR instruction that jumps to a 32 register value. However, given the performance of the device, this amount of memory should be plenty for a while.

- 64 MiW (256 MiB) DDR3 SDRAM (although twice is physically available if needed)
- 32 MiW (128 MiB) micro sd flash (more available if needed)
- 2x 8 MiW (2x 32 MiB) spi flash

## I/O (WIP)

- Memory mapped I/O

## Performance (WIP)

- ? Instructions per second on average
