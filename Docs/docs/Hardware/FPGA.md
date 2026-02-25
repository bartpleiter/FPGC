# FPGA

The FPGC uses an Intel/Altera Cyclone IV EP4CE40 FPGA on a custom PCB. This page covers the FPGA specs, clock configuration, and requirements.

## Cyclone IV EP4CE40

The EP4CE40F23I7N was chosen for its low cost (around 20 euros from LCSC) and sufficient resources. The I7 speed grade provides enough timing margin for 100 MHz operation.

| Specification | Value |
|---|---|
| Logic Elements | 39,600 |
| Block RAM | 1.1 Mbit |
| HW Multipliers | 116 |
| Package | 484-pin FBGA |

With the pixel framebuffer in external SRAM, BRAM usage is modest: the L1 caches (2x 128 lines of 271 bits), ROM, and dual-port VRAM fit comfortably.

## Clock Domains

The PLL generates four clocks from a 50 MHz input:

| Clock | Frequency | Purpose |
|---|---|---|
| `clk100` | 100 MHz | CPU, cache controller, SDRAM controller, SPI masters, Memory Unit |
| `clkSDRAM` | 100 MHz (180° phase shift) | SDRAM chip clock. Phase shift centers data sampling. |
| `clkGPU` | 25 MHz | Pixel clock for 640x480 VGA timing, GPU rendering |
| `clkTMDShalf` | 125 MHz | HDMI TMDS encoding (5x pixel clock, using DDR for 10-bit symbols) |

All clocks are derived from the same PLL, so they are phase-aligned. This simplifies clock domain crossings between the 100 MHz and 25 MHz domains (single-register synchronization is sufficient).
