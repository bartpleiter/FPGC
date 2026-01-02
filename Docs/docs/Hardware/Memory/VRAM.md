# VRAM

FPGC uses multiple types of video RAM for different purposes:

- **VRAM32**: 32-bit wide dual-port block RAM for background/window tile maps
- **VRAM8**: 8-bit wide dual-port block RAM for pattern tables
- **VRAMPX (External SRAM)**: Pixel framebuffer using external IS61LV5128AL SRAM

## VRAM32 and VRAM8

Both VRAM32 and VRAM8 are implemented using the FPGA's internal dual-port block RAMs (M9K blocks). These memories are used for the tile based rendering. As the rendering strategy using tile maps and pattern tables uses so little memory, internal block RAM will most likely never be a bottleneck.

## Pixel Framebuffer (VRAMPX)

The pixel framebuffer stores raw R3G3B2 pixel data (8 bits per pixel) in an external SRAM chip. This provides a 320×240 resolution framebuffer that is scaled to 640×480 for VGA output.

### Architecture Overview

!!! TODO
    Decide what to document here and what to put in the GPU/FSX documentation instead.

!!! Warning
       This is already outdated due to the CPU now running at 100MHz. Module needs to be updated as the CPU is now so fast that some writes are lost.

The pixel framebuffer architecture makes use of the synchronized clock domain between the CPU (50MHz) and GPU (25MHz) to simplify the design. Key features include:

- Arbiter that switches between CPU writes and GPU reads depending on active/blanking periods
- Write FIFO to buffer CPU writes during active video periods
- GPU reads directly from SRAM via the arbiter during active video

```text
┌──────────────┐                           ┌─────────────────┐
│     CPU      │                           │  External SRAM  │
│   (50MHz)    │                           │ IS61LV5128AL    │
└──────┬───────┘                           │  512K × 8-bit   │
       │                                   │  10ns access    │
       │ Write                             └────────┬────────┘
       ▼                                            │
┌──────────────┐                                    │
│ Write FIFO   │     ┌──────────────┐               │
│ (M9K Block)  │────▶│   Arbiter    │◄──────────────┘
│ 512 entries  │     │  (100MHz)    │
└──────────────┘     └──────┬───────┘
                            │
                            │ Read (direct)
                            ▼
                     ┌──────────────┐
                     │ PixelEngine  │
                     │   (25MHz)    │
                     │  +Line Buf   │
                     └──────────────┘
```

### Clock Domains

| Clock | Frequency | Purpose |
| ------- | ----------- | --------- |
| clk100 | 100 MHz | Arbiter, SRAM timing |
| clk50 | 50 MHz | CPU, Write FIFO (write side) |
| clkPixel | 25 MHz | GPU, Pixel Engine |

All clocks are generated from the same PLL and are phase-aligned, which simplifies clock domain crossing.

### Write Path (CPU → SRAM)

1. CPU writes pixel data (address + color) to the **Write FIFO** at 50MHz
2. FIFO stores entries in M9K block RAM (512 entries deep)
3. During **blanking periods**, the arbiter drains the FIFO to SRAM
4. Each SRAM write takes 2 arbiter cycles (100MHz):
   - Cycle 1: Set up address and data
   - Cycle 2: Assert WE_n, complete write

### Read Path (SRAM → GPU)

1. During **active video**, the arbiter dedicates SRAM to GPU reads
2. GPU's `PixelEngineSRAM` outputs the current pixel address
3. Arbiter continuously reads from SRAM address
4. Data is registered for stability and passed to Pixel Engine
5. **Line buffer** provides vertical 2× scaling (same line repeated twice)

### Scaling

The display uses 2× scaling in both dimensions to create a 640×480 output from a 320×240 framebuffer.
This is needed to match standard VGA resolution (not all displays support 320×240 natively).

Horizontal scaling is achieved by reading each pixel twice (same address for 2 VGA pixels).

Vertical scaling uses a **line buffer**: the first line is read from SRAM and stored in the buffer, then the second line is read from the buffer instead of SRAM.

### Modules

| Module | Description |
| -------- | ------------- |
| `AsyncFIFO.v` | Dual-clock write FIFO with Gray-coded pointers for robust CDC |
| `SRAMArbiter.v` | 100MHz arbiter - writes during blanking, reads during active |
| `VRAMPXSram.v` | Top-level wrapper connecting FIFO, arbiter, and SRAM |
| `PixelEngineSRAM.v` | Generates pixel addresses, handles scaling |
| `FSX_SRAM.v` | Frame synthesizer using direct SRAM interface |

### Clock Domain Crossing

The write FIFO crosses from the 50MHz CPU domain to the 100MHz arbiter domain. While the clocks should be synchronized, data corruption still happens when there is no proper synchronization. Therefore, the FIFO implemented is a true dual-clock FIFO.

### SRAM Chip: IS61LV5128AL

- **Capacity**: 512K × 8 bits
- **Access Time**: 10ns
- **Interface**: Active-low control signals (CE_n, OE_n, WE_n)
- **Usage**: Lower 76,800 bytes used for pixel framebuffer

### Timing Constraints

With the arbiter running at 100MHz (10ns period) and the SRAM having 10ns access time:

1. **Read**: Address stable → OE_n low → data valid after 10ns
2. **Write**: Address + data stable → WE_n pulse → data latched on WE_n rising edge

The design provides exactly one arbiter cycle for each SRAM access, matching the SRAM's capabilities.
