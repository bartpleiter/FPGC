# GPU

The FPGC GPU, called FSX (Frame Synthesizer), generates a 640×480 HDMI video signal at 60Hz from two independently rendered graphics layers. It doesn't have a sophisticated rendering pipeline like a modern GPU. Instead, it's closer to a classic console PPU with an added pixel framebuffer.

## Architecture at a Glance

The GPU has two rendering planes that are composited together:

1. **BGW plane**: a tile-based renderer for backgrounds and text (inspired by the NES)
2. **Pixel plane**: a simple bitmap framebuffer stored in external SRAM

BGW gets priority. When a BGW pixel is black (RGB = 0,0,0), the pixel plane shows through instead. This gives a cheap color-key transparency without any per-pixel alpha blending hardware.

Both planes render at 320x240 and are scaled up 2× to fill the 640×480 output.

## Timing

The GPU runs off a 25 MHz pixel clock derived from the system PLL. Standard 640×480 VGA timing:

- 800 total pixels per line (640 active + 160 blanking)
- 525 total lines per frame (480 active + 45 blanking)
- ~60 Hz refresh rate

A `frame_drawn` interrupt is sent to the CPU at the start of each vertical blanking period, so software can synchronize rendering to avoid tearing or as a simple frame counter or timer.

## BGW Plane (Tile Renderer)

The Background/Window renderer draws two layers of 8×8 pixel tiles, displayed at 2× scale (so each tile appears as 16×16 on screen). This gives a visible grid of 40×25 tiles covering 320×200 logical pixels. The remaining 40 lines at the bottom of the screen are unused by this plane.

### Why Tiles?

Tile rendering requires far less memory than a full framebuffer. Instead of storing 76,800 individual pixel colors, you store a small set of reusable tile patterns and a grid of indices saying which tile goes where. The CPU just writes a tile index to change an 8×8 region, which is great for text, UI, and backgrounds. Even more importantly, this saves a bunch of CPU complexity compared to just software rendering into a pixel framebuffer.

### Two Layers

- **Background (BG):** 64 tiles wide (wraps around), supports horizontal scrolling at both tile granularity and fine per-pixel offsets. This is used for scrolling game worlds. This was also implemented by the NES PPU.
- **Window:** 40 tiles wide, no scrolling, draws on top of the background. Useful for fixed UI elements like score displays or text overlays, a perfect fit for a terminal output as well! A window pixel is transparent when its pattern bits are zero and its palette's first color is black.

### Memory Layout

The tile renderer reads from two on-chip dual-port RAMs:

**VRAM8 (8-bit, 16K entries)**, containing tile maps and scroll registers:

| Range | Content |
|---|---|
| 0–2047 | BG tile indices (64×32 grid, 1 byte each) |
| 2048–4095 | BG color/palette indices |
| 4096–6143 | Window tile indices (40×25 grid) |
| 6144–8191 | Window color/palette indices |
| 8192 | Horizontal tile scroll offset |
| 8193 | Horizontal fine scroll offset (0–7 pixels) |

**VRAM32 (32-bit, ~1K entries)**, containing tile patterns and palettes:

| Range | Content |
|---|---|
| 0–1023 | Pattern data (256 tiles × 4 words; each word encodes 2 rows of 8 pixels at 2 bits per pixel) |
| 1024+ | Palette entries (each 32-bit word holds 4 colors in R3G3B2 format) |

### Rendering Pipeline

For each tile on screen, the renderer executes an 8-phase fetch sequence (one fetch per pixel clock cycle), reading BG data first then Window data. The phases are interleaved to give enough time for both layers:

1. Read BG tile index → 2. Read BG pattern → 3. Read BG palette index → 4. Read BG palette → 5–8. Same for window layer

The result is a stream of 8-bit R3G3B2 colors at the pixel clock rate.

### Scrolling

Background scrolling uses a shift-register buffer that holds one tile's worth of pixel data. The `x_fine_offset` (0–7) selects a tap position in the buffer, enabling smooth sub-tile scrolling. Combined with the `x_tile_offset`, you get full horizontal scrolling:

- **Coarse scroll:** shift the starting tile index
- **Fine scroll:** shift the tap position in the pixel buffer

Vertical scrolling isn't supported in hardware. You'd need to rewrite the tile map in software.

## Pixel Plane (Bitmap Framebuffer)

For games that need per-pixel control (like Doom), the BGW renderer doesn't cut it. The Pixel Plane provides a straightforward 320×240 framebuffer where each pixel is individually addressable.

### Pixel Format

Each pixel is an 8-bit index into a **programmable 256-entry color palette**. The framebuffer lives in external SRAM at CPU addresses `0x7B00000` through `0x7B12BFF`. Pixels are stored linearly:

```
address = 0x7B00000 + (y × 320) + x
```

### Color Palette

The pixel plane has a dedicated 256×24-bit color palette stored in on-chip dual-port BRAM. Each framebuffer pixel value (0–255) is used as an index into this palette to produce a 24-bit RGB color for display.

The palette is memory-mapped at CPU addresses `0x7B20000` through `0x7B200FF`:

```
palette_address = 0x7B20000 + index    (index 0–255)
```

Each entry holds a 24-bit RGB value in the format `0x00RRGGBB`. The GPU reads the palette at the pixel clock rate (25 MHz) with a registered output, adding one pixel clock cycle of latency. Sync signals (blank, hsync, vsync) are delayed by a matching register stage to keep everything aligned.

At power-on, the palette is initialized to a default R3G3B2 color mapping using bit-replication (e.g., a 3-bit red value of `0b101` becomes `0b10110110`), so without any software intervention the palette behaves identically to a classic R3G3B2 framebuffer. Software can reprogram any or all entries at runtime to implement custom color schemes, palette cycling, fade effects, or optimized palettes for specific content.

### The SRAM Sharing Problem

The pixel framebuffer lives in a single external SRAM chip (IS61LV5128AL, 512K×8) for two reasons:

- If you want to keep the entire framebuffer in FPGA BRAM, the amount of BRAM becomes the bottleneck in selecting an FPGA for this project, limiting the FPGAs this project can target. The EP4CE40 that this project uses just barely does not fit when you include all other BRAM needs.
- Using BRAM feels like cheating as it makes implementation trivial, and this project basically compares to ~1990 hardware. Dual port dual clock SRAM of this size was basically not an option back then.

Both the GPU and CPU need access. The GPU reads pixels for display, the CPU writes pixels to draw. Since the SRAM has only one port, they can't both access it simultaneously.

The solution is **time-division multiplexing** based on scanline parity, enabled by a line buffer trick:

- **Even scanlines:** GPU reads from SRAM. Since we're doing 2× vertical scaling, even and odd output lines display the same source row.
- **Odd scanlines:** GPU replays the data it already read (from an internal 320-byte line buffer). SRAM is free for CPU writes.
- **Blanking periods:** No pixels needed, SRAM is free for CPU writes.

This gives the CPU write access during about 65% of each frame, and at a higher clock rate than the GPU uses, so it can easily write frames faster than the GPU can read.

### Write Path

CPU writes go through a 1024-entry FIFO that buffers them at 100 MHz. A SRAM arbiter drains the FIFO during available time windows (blanking and odd scanlines), writing entries to SRAM at a rate of one every 3 clock cycles (~33 million pixels/second). If the FIFO fills completely, the CPU pipeline stalls until space opens up. This prevents any writes from being silently dropped. Note that this will basically not happen given the speed, but this design is future-proofed if I were to replace the SRAM with SDRAM to get a larger framebuffer at the cost of latency. This would be needed for higher color depths or multiple planes.

## HDMI Output

The final composited pixel passes through the pixel-plane color palette (a 256×24-bit lookup table) to produce a 24-bit RGB value, which is then TMDS-encoded for HDMI output. The default palette entries use R3G3B2 bit-replication, so out of the box the output looks identical to the original fixed R3G3B2 scheme.

### TMDS on Cyclone IV

The Cyclone IV FPGA doesn't have native TMDS serializers, but at 480p the data rate is manageable with a workaround: a 125 MHz clock (5× the pixel clock) combined with DDR output gives the required 250 Mbps per channel. A self-rotating 10-bit shift register loads a TMDS symbol every 5 clock cycles and shifts out 2 bits per cycle through DDR primitives. AC coupling capacitors on the board handle the DC offset. It works reliably on every monitor tested so far. The nice thing is that it also works with HDMI to VGA converters, allowing extremely clean output on CRT monitors as well, which fits the retro vibe of the project.
