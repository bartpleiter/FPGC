# FPGC Camera Context (for AI coding tools)

Game Boy Camera-inspired digital camera built on the FPGC platform.
Bare-metal application (no BDOS) using the B32P3 CPU, OV7670 sensor,
DMA-accelerated display pipeline, and SD card storage. For the
project-wide overview see [Project-context.md](Project-context.md).

## What it is

A standalone monochrome camera that captures, processes, and stores
images on an SD card. Live viewfinder at up to 30 fps with
hardware-accelerated dithering. Two hardware targets:

| Target | FPGA | Display | Status |
|--------|------|---------|--------|
| Prototype | Cyclone IV EP4CE40 (custom PCB) | HDMI 640×480 | Working PoC |
| Camera PCB | Cyclone 10 LP CL120 (core board) | ILI9341 SPI TFT 320×240 | Verilog prepared |

## Build & run

| Target | Purpose |
|--------|---------|
| `make compile-camera` | Compile camera binary (`Software/ASM/Output/code.bin`) |
| `make run-camera` | Compile + upload via UART |
| `make flash-camera` | Compile + flash to SPI (persistent boot) |
| `make sd-read-brfs dev=/dev/sdX` | Extract BRFS filesystem from SD to `Files/BRFS-sd-transfer/` |
| `make sd-write-brfs dev=/dev/sdX` | Write `Files/BRFS-sd-transfer/` to SD card |

Pipeline: `crt0_baremetal.asm + libc + libfpgc + camera sources →
cproc → QBE → asm → ASMPY linker → .list → .bin`.

**Validation rule:** After editing any file under `Software/C/camera/`
or `Software/C/libfpgc/`, always run `make compile-camera` to verify.

## Source layout

### Camera application (`Software/C/camera/`)

| File | Lines | Role |
|------|-------|------|
| `main.c` | ~224 | Entry point, boot sequence, interrupt handler |
| `cam_driver.c/h` | ~75 | Camera MMIO register wrappers |
| `ov7670_init.c/h` | ~206 | OV7670 sensor configuration via I2C |
| `viewfinder.c/h` | ~800 | Live display loop, DMA blitting, capture, key handling |
| `image_proc.c/h` | ~230 | Auto-contrast, dithering algorithms |
| `settings.c/h` | ~340 | Camera settings state (Auto/Manual modes) |
| `hud.c/h` | ~194 | HUD overlay on GPU window tile layer |
| `bmp.c/h` | ~181 | BMP encoder/decoder (8-bit greyscale) |
| `storage.c/h` | ~148 | SD card init, BRFS mount/format, image tracking |
| `gallery.c/h` | ~248 | Image browser with delete |

### libfpgc dependencies (used by camera build)

| Driver | Purpose |
|--------|---------|
| `io/dma.c` + `dma_asm.asm` | DMA engine (CAM2MEM, CAM2VRAM, MEM2VRAM) |
| `io/i2c.c` | I2C master for OV7670 SCCB configuration |
| `io/spi.c` | SPI bus control |
| `io/sd.c` | SD card SPI-mode driver |
| `io/timer.c` | Hardware timers (USB polling, delay) |
| `io/uart.c` | UART debug output |
| `io/ch376.c` | CH376 USB keyboard driver |
| `gfx/gpu_hal.c` | GPU palette/VRAM writes |
| `gfx/gpu_data_ascii.c` | ASCII font pattern data |
| `fs/brfs.c` | BRFS v2 filesystem core |
| `fs/brfs_cache.c` | LRU block cache |
| `fs/brfs_storage_sdcard.c` | SD card storage backend |

### Verilog hardware

| Path | Target |
|------|--------|
| `Hardware/FPGA/CycloneIV_EP4CE40/` | Prototype (Cyclone IV, HDMI) |
| `Hardware/FPGA/Cyclone10_CL120/` | Camera PCB (Cyclone 10, SPI display) |
| `Hardware/FPGA/Verilog/Modules/IO/CameraCapture.v` | OV7670 capture engine |
| `Hardware/FPGA/Verilog/Modules/IO/I2C_master.v` | I2C/SCCB controller |
| `Hardware/FPGA/Verilog/Modules/IO/DMAengine.v` | DMA with camera modes |
| `Hardware/FPGA/Verilog/Modules/IO/ButtonInput.v` | Debounced button input |
| `Hardware/FPGA/Verilog/Modules/GPU/SPIDisplayController.v` | ILI9341 SPI display |
| `Hardware/FPGA/Verilog/Modules/GPU/ILI9341_Init.v` | Display init sequencer |
| `Hardware/FPGA/Verilog/Modules/GPU/FrameScanEngine.v` | Framebuffer scanner |
| `Hardware/FPGA/Verilog/Modules/GPU/SPIMaster.v` | 50 MHz SPI transmitter |
| `Hardware/FPGA/Verilog/Modules/GPU/PixelPalette.v` | 256-entry RGB24 LUT |

### Design documents (`Docs/plans/`)

| File | Content |
|------|---------|
| `fpgc-camera.md` | V3 master design document |
| `fpgc-camera-poc-v1-review-and-v2-plan.md` | V1 postmortem, V2 architecture |
| `fpgc-camera-poc2-full-documentation.md` | V2 as-built documentation |
| `camera-settings-plan.md` | Settings/exposure control design |
| `debug-camera-writes.md` | SDRAM crash diagnosis + DMA fix |
| `spidisplay-plan.md` | ILI9341 SPI display replacement |

There are more documents in that folder, they start with "fpgc-camera".

## Hardware architecture

### OV7670 sensor

- **Interface:** 8-bit parallel data, PCLK, VSYNC, HREF, XCLK
- **Configuration:** SCCB (I2C-compatible), address `0x21`
- **Output format:** YUV422 (UYVY byte order)
- **Resolutions:** QVGA 320×240, QQVGA 160×120 (via 4× DCW)
- **Master clock:** 25 MHz from FPGA PLL (`clk25`)

### Camera data path

```
OV7670 sensor (QVGA YUV422, 30 fps)
    ↓  8-bit parallel + PCLK/VSYNC/HREF
CameraCapture.v
    • 2-stage sync from PCLK to 100 MHz domain
    • Extract Y channel (luminance) from UYVY stream
    • Assemble 256-bit cache lines (32 Y pixels)
    • Handshake: line_ready / line_data / line_ack
    • Double-buffered frames (current_buf toggles on VSYNC)
    ↓
DMA engine (CAM2MEM or CAM2VRAM mode)
    • Consume cache lines via handshake
    • CAM2MEM: write Y data to SDRAM frame buffer
    • CAM2VRAM: write to VRAMPX with inline LUT + dither + upscale
    ↓
Display (HDMI or SPI TFT)
```

### Display pipeline

**Prototype (Cyclone IV — HDMI):**
- GPU pixel framebuffer (VRAMPX): 320×240, 8-bit indexed
- GPU window tile layer: 40×25 text (HUD overlay, transparent)
- HDMI output: 640×480 (2× upscaled by GPU)

**Camera PCB (Cyclone 10 — SPI TFT):**
- Internal BRAM pixel framebuffer: 320×240, 8-bit indexed
- FrameScanEngine reads pixels → PixelPalette → RGB565 → SPIMaster
- ILI9341 TFT: 320×240 native (no upscaling needed)
- SPI clock: 50 MHz, ~32 fps

### I2C / SCCB

Hardware I2C master (`I2C_master.v`). Software writes 32-bit command
word to `FPGC_I2C_CMD`:

```c
// Write OV7670 register:
i2c_write(0x21, reg, val);
// Encodes: {dev_addr[23:17], rw[16], reg[15:8], data[7:0]}
```

**Design convention:** I2C writes are performed with camera capture
disabled, even though it is not strictly required by the hardware.
This is a deliberate design choice for clean frame boundaries. All
OV7670 register changes use the deferred I2C pattern: keypress sets
a pending flag → after DMA completes → `cam_disable()` → apply I2C
writes → `cam_enable_phase(1)`.

### Button input (Cyclone 10 only)

`ButtonInput.v`: 32-channel active-low GPIO with 2-stage sync and
~20 ms debounce. CPU reads `FPGC_BTN_STATE` (`0x1C0000B4`).

## MMIO registers

### Camera capture

| Address | Register | Bits |
|---------|----------|------|
| `0x1C000088` | `FPGC_CAM_CTRL` | `[0]` enable, `[1]` byte_phase |
| `0x1C00008C` | `FPGC_CAM_STATUS` | `[0]` frame_done (R/C), `[1]` cur_buf, `[3]` vsync, `[4]` href |
| `0x1C000094` | `FPGC_CAM_BUF0` | Debug: `[16:0]` frame_pixels, `[25:17]` line_count |
| `0x1C000098` | `FPGC_CAM_BUF1` | Debug: `[11:0]` cache_lines, `[19:12]` partial_drops |
| `0x1C00009C` | `FPGC_CAM_DBG` | Debug: `[2:0]` state, `[3]` arb_busy |

### I2C

| Address | Register | Access |
|---------|----------|--------|
| `0x1C0000A0` | `FPGC_I2C_CMD` | W: `{dev_addr[23:17], rw[16], reg[15:8], data[7:0]}` |
| `0x1C0000A4` | `FPGC_I2C_DATA` | R: `{24'd0, rd_data[7:0]}` |
| `0x1C000090` | `FPGC_I2C_DBG` | R: `[4:0]` state, `[5]` start, `[6]` pending, `[7]` busy |

### DMA (camera-relevant modes & flags)

| Address | Register |
|---------|----------|
| `0x1C000070` | `FPGC_DMA_SRC` |
| `0x1C000074` | `FPGC_DMA_DST` |
| `0x1C000078` | `FPGC_DMA_COUNT` |
| `0x1C00007C` | `FPGC_DMA_CTRL` |
| `0x1C000080` | `FPGC_DMA_STATUS` |
| `0x1C0000AC` | `FPGC_DMA_LUT` — W: `{addr[15:8], data[7:0]}` |
| `0x1C0000B0` | `FPGC_DMA_DITHER` — W: `{table[13:12], mi[11:8], data[7:0]}` |

**DMA modes:**

| Mode | ID | Direction | Usage |
|------|----|-----------|-------|
| `CAM2MEM` | 7 | Camera → SDRAM | Still capture to buffer |
| `CAM2VRAM` | 8 | Camera → VRAMPX | Live viewfinder display |
| `MEM2VRAM` | 3 | SDRAM → VRAMPX | Gallery display, fallback blit |

**DMA control flags (bits in `FPGC_DMA_CTRL`):**

| Flag | Bit | Purpose |
|------|-----|---------|
| `FPGC_DMA_CTRL_CAM_IMM` | 8 | Skip frame_done wait (immediate capture) |
| `FPGC_DMA_CTRL_LUT_EN` | 9 | Apply 256-entry auto-contrast LUT |
| `FPGC_DMA_CTRL_DITHER_EN` | 10 | Apply ordered dithering |
| `FPGC_DMA_CTRL_DITHER_8` | 11 | Dither mode: 0 = 4-shade, 1 = 8-shade |
| `FPGC_DMA_CTRL_UPSCALE2X` | 12 | 2× nearest-neighbor pixel doubling |

### Buttons (Cyclone 10)

| Address | Register |
|---------|----------|
| `0x1C0000B4` | `FPGC_BTN_STATE` — `[31:0]` debounced state (active-high, read-only) |

## Memory map

| Region | Address | Size | Purpose |
|--------|---------|------|---------|
| Code + BSS | `0x00000000` | ~224 KB | Bare-metal program |
| Stack | `0x00107FFC` | grows down | Main stack |
| Camera buffer 0 | `0x02000000` | 76,800 B | CAM2MEM double-buffer A |
| Camera buffer 1 | `0x02012C00` | 76,800 B | CAM2MEM double-buffer B |
| Capture buffer | `0x02600000` | 76,800 B | Staging for BMP save |
| Process buffer | `0x02700000` | 76,800 B | Dithered output before save |
| BRFS cache | `0x02800000` | 24 MiB | SD card filesystem cache |

Frame size: 320×240 = 76,800 bytes (Y channel only).
QQVGA frame: 160×120 = 19,200 bytes.

## Boot sequence

```
1. gpu_clear_vram()           — clear bootloader logo
2. hud_init()                 — load ASCII font patterns + palettes
3. load_dither_tables()       — upload Dashboy/Bayer tables to DMA
4. timer_init()               — needed for USB CH376 delays
5. keyboard_init()            — CH376 USB keyboard on SPI_USB_1
6. storage_init()             — SD card init, mount BRFS
   → prompt user to format if no BRFS found
7. ov7670_init_mode(0)        — QVGA 320×240 YUV422
8. settings_init()            — defaults: Auto, ISO 800, night on
9. cam_enable_phase(1)        — start capture (phase=1: even bytes)
10. wait for VSYNC            — clean frame boundary
11. viewfinder_run(MODE_RAW)  — enter main loop (never returns)
```

Interrupts: Timer 1 (USB HID polling), Timer 2 (`delay()`).

## Display modes

| Mode | Key | Shades | Description |
|------|-----|--------|-------------|
| `MODE_RAW` | `R` | 256 | Direct Y luminance (greyscale) |
| `MODE_DITH` | `D` | 4 | Dashboy ordered dither (Game Boy palette) |
| `MODE_DITH8` | `E` | 8 | Bayer 4×4 ordered dither |

| Resolution | Key | Sensor | Display | BMP size |
|------------|-----|--------|---------|----------|
| QVGA | — | 320×240 | 1:1 native | 77,878 B (20 blocks) |
| QQVGA | `Q` | 160×120 | 2× hardware upscale | 20,278 B (5 blocks) |

## Camera settings

### Shooting modes

| Mode | Abbrev | User controls | Auto-adjusts |
|------|--------|---------------|--------------|
| Auto | `[A]` | Brightness, contrast | Exposure, gain, frame rate |
| Manual | `[M]` | Shutter, exposure, ISO, brightness, contrast | Nothing |

### Settings struct

```c
typedef struct {
    int shoot_mode;     /* SHOOT_AUTO / SHOOT_M */
    int shutter;        /* SHUTTER_FAST / NORMAL / SLOW / SLOWER */
    int exposure;       /* EXPOSURE_FULL / HALF / QUARTER / EIGHTH / SIXTEENTH */
    int iso;            /* ISO_100..ISO_3200 */
    int brightness;     /* -128..+127 */
    int contrast;       /* 0..127 */
    int night_mode;     /* NIGHT_OFF / HALF / QUARTER / EIGHTH */
    int mirror;         /* 0 / 1 */
    int flip;           /* 0 / 1 */
    int show_hud;       /* 0 / 1 */
    int auto_contrast;  /* 0 / 1 (hardware LUT stretch) */
} camera_settings_t;
```

### OV7670 register mapping

| Setting | Register | Values |
|---------|----------|--------|
| Shutter speed | `CLKRC` (0x11) | `0x80` (30fps), `0x00` (16fps), `0x01` (8fps), `0x03` (4fps) |
| Exposure lines | `AECH` (0x10) | `0x78` (480), `0x3C` (240), `0x1E` (120), `0x0F` (60), `0x07` (30) |
| ISO / Gain | `GAIN` (0x00) | `0x00`–`0xFF` (manual), ceiling via `COM9` (auto) |
| Brightness | `BRIGHT` (0x55) | Sign-magnitude: bit 7 = sign, bits 6:0 = magnitude |
| Contrast | `CONTRAS` (0x56) | `0x00`–`0x7F` |
| Night mode | `COM11` (0x3B) | OFF=`0x0A`, 1/2=`0x2A`, 1/4=`0x4A`, 1/8=`0xEA` |
| Mirror/Flip | `MVFP` (0x1E) | `[5]` mirror, `[4]` flip |

## Keyboard controls

### Viewfinder

| Key | Action | Notes |
|-----|--------|-------|
| `R` | RAW mode (256-shade) | |
| `D` | DITH mode (4-shade) | |
| `E` | DITH8 mode (8-shade) | |
| `Q` | Toggle QVGA ↔ QQVGA | Requires sensor reinit |
| `M` | Toggle Auto ↔ Manual | |
| `Space` | Capture image | Deferred until DMA completes |
| `G` | Enter gallery | Stops camera |
| `[` / `]` | Shutter speed ±  | Manual only |
| `{` / `}` | Exposure lines ± | Manual only |
| `-` / `=` | ISO ± | Manual only |
| `9` / `0` | Brightness ↓/↑ | |
| `7` / `8` | Contrast ↓/↑ | |
| `X` / `Y` | Mirror / Flip toggle | |
| `H` | Toggle HUD | |
| `L` | Toggle auto-contrast LUT | |
| `` ` `` | Reset all settings | |

### Gallery

| Key | Action |
|-----|--------|
| `.` / `,` | Next / previous image |
| `D` | Delete current image |
| `G` / `Escape` | Exit to viewfinder |

## Image capture flow

1. User presses `Space` → sets pending capture flag
2. Wait for current DMA frame to complete
3. `cam_disable()` — stop sensor capture
4. Enable camera for one-shot frame via `CAM2MEM` DMA
5. DMA writes Y data to `CAPTURE_BUF` (`0x02600000`)
6. If dithering enabled:
   - Apply auto-contrast LUT (if active) via CPU
   - Dither via CPU to `PROCESS_BUF` (`0x02700000`)
   - Map dither indices to greyscale values
   - Save `PROCESS_BUF` as BMP
7. If RAW mode:
   - Apply auto-contrast LUT if active
   - Save `CAPTURE_BUF` directly as BMP
8. `bmp_save()`: BMP header + 256-entry greyscale palette + pixels
   → BRFS write to `/DCIM/IMG_NNNN.BMP`
9. `storage_sync()` — flush BRFS cache to SD card
10. `cam_enable_phase(1)` — resume live viewfinder

## Image storage

| Parameter | Value |
|-----------|-------|
| Format | 8-bit greyscale BMP (BITMAPINFOHEADER) |
| Directory | `/DCIM/` on BRFS |
| Naming | `IMG_NNNN.BMP` (auto-increment) |
| BRFS block size | 4,096 bytes |
| BRFS total blocks | 6,144 (24 MiB partition) |
| BRFS label | `"camera"` |
| QVGA file size | 77,878 bytes (20 blocks) |
| QQVGA file size | 20,278 bytes (5 blocks) |
| Capacity | ~305 QVGA or ~1,220 QQVGA images |

## HUD overlay

Uses GPU window tile layer (transparent overlay on pixel layer).

```
Row 0:  [A]                                SD  AC
        Mode                          SD status  Auto-contrast
Row 24: ISO:200  B:+000 C: 64    305img  30fp
        Gain     Bright  Contrast  Remaining  FPS
```

Palettes: white-on-black (text), green-on-black (active indicators),
red-on-black (SD errors).

## Viewfinder loop (critical pattern)

The main loop is a tight DMA-driven frame pipeline:

```
1. Start CAM2VRAM DMA (with flags: LUT_EN, DITHER_EN, UPSCALE2X)
2. While DMA busy:
   - Poll USB keyboard (ch376_read_int)
   - Queue keypresses (deferred I2C pattern)
3. DMA completes:
   - Handle pending capture (if Space was pressed)
   - cam_disable() → apply deferred I2C writes → cam_enable_phase(1)
   - Periodically update auto-contrast LUT from DMA min/max stats
   - Update HUD every N frames
   - Measure FPS
4. Repeat
```

**Deferred I2C pattern:** OV7670 registers must be written with
capture stopped. Key presses set flags, I2C writes happen in the
gap between DMA completion and next frame start.

## Hardware pin mapping

### Prototype (Cyclone IV EP4CE40)

All 16 spare pins used (zero pins remaining):

| Signal | FPGA Pin |
|--------|----------|
| `cam_data[0:7]` | `gpio_1` – `gpio_8` |
| `cam_vsync` | `gpio_9` |
| `cam_href` | `disp_6` |
| `cam_pclk` | `sys_clk_header` (clock-capable) |
| `cam_xclk` | `disp_1` (25 MHz from PLL) |
| `cam_sioc` | `disp_3` |
| `cam_siod` | `disp_4` (open-drain + external pullup) |
| `cam_reset_n` | `disp_5` |
| `cam_pwdn` | `disp_2` |

### Camera PCB (Cyclone 10 CL120)

Dedicated pins on custom PCB:

| Signal | Purpose |
|--------|---------|
| `cam_data[7:0]` | Parallel pixel data D0–D7 |
| `cam_vsync` | Frame sync |
| `cam_href` | Line valid |
| `cam_pclk` | Pixel clock from sensor |
| `cam_xclk` | 25 MHz master clock (from PLL `clk25`) |
| `cam_reset_n` | Active-low reset |
| `cam_pwdn` | Power down (driven LOW = active) |
| `cam_scl` | SCCB clock (push-pull) |
| `cam_sda` | SCCB data (open-drain) |

SPI display pins (reuse display header):

| Signal | Purpose |
|--------|---------|
| `spi_clk` | 50 MHz SPI clock |
| `spi_mosi` | Serial data |
| `spi_cs_n` | Chip select |
| `spi_dc` | Data/command |
| `lcd_rst_n` | Display reset |
| `lcd_backlight` | Backlight enable |

## Input

**Prototype (Cyclone IV):** USB keyboard via CH376 on `FPGC_SPI_USB_1`.
All viewfinder/gallery controls mapped to keyboard keys.

**Camera PCB (Cyclone 10):** Physical push buttons via `ButtonInput.v`
module (debounced GPIO, read via `FPGC_BTN_STATE`). No USB keyboard.
The button-to-action mapping needs to be implemented when porting to
the Cyclone 10 target.

## SPI display limitations

The Cyclone 10 SPI display controller currently implements **only the
pixel framebuffer (VRAMPX)** — the window tile layer is not yet
composited. Adding window layer support to FrameScanEngine is planned
(see `Docs/plans/fpgc-camera-spi-hud.md`). The Cyclone IV prototype
keeps HDMI with the existing tile-based HUD and does not need changes.

## Key differences: Cyclone IV vs Cyclone 10

| Feature | Cyclone IV (prototype) | Cyclone 10 (camera PCB) |
|---------|----------------------|------------------------|
| Pixel framebuffer | External SRAM (128 KB) | Internal BRAM (76.8 KB) |
| Display | HDMI 640×480 (2× upscale) | ILI9341 SPI TFT 320×240 |
| Camera pins | GPIO/display header | Dedicated PCB pins |
| Input | USB keyboard (CH376) | Debounced push buttons |
| SDRAM | Single 32-bit chip | Dual 16-bit chips |
| Ethernet | ENC28J60 (present) | Removed |
| USB ports | 2× CH376 | Removed (or 1 for debug) |

## Clock domains

| Clock | Freq | Source | Purpose |
|-------|------|--------|---------|
| `clk100` | 100 MHz | PLL c1 | CPU, DMA, camera capture, all logic |
| `clkSDRAM` | 100 MHz (phase-shifted) | PLL c2 | SDRAM interface |
| `clk25` | 25 MHz | PLL c3 | OV7670 XCLK master clock |
| `clkTMDShalf` | 125 MHz | PLL c4 | HDMI TMDS (Cyclone IV only) |

OV7670 PCLK (~25 MHz) is sampled as data in the 100 MHz domain
with 2-stage synchronizers.

## Dangerous areas — ask the user before changing

- Camera buffer addresses in `cam_driver.h` (tied to SDRAM layout)
- BRFS cache base address in `storage.c` (must not overlap buffers)
- CameraCapture Y-channel extraction logic (byte_phase / byte_toggle)
- DMA CAM2MEM / CAM2VRAM state machines in `DMAengine.v`
- OV7670 init register sequence in `ov7670_init.c`
- I2C command encoding format (`FPGC_I2C_CMD` bit layout)

## Long-term goal

Build a self-contained handheld camera device — a modern take on the
Game Boy Camera. The final product is a custom PCB with the Cyclone 10
LP core board as the FPGA module, OV7670 sensor, ILI9341 SPI TFT
display, push buttons, SD card slot, and battery power. No external
monitor, no USB keyboard — a fully portable device.

The Cyclone IV prototype validates the full software stack and
camera pipeline before committing to PCB design. Once the PoC is
feature-complete, the hardware design moves to a dedicated camera
PCB with only the peripherals needed (no Ethernet, no HDMI, no
dual USB ports).

## Coding guidelines

- **Bare-metal** — no BDOS, no kernel, no syscalls. Direct MMIO.
- C11 with cproc/QBE limits: no `volatile`, no inline asm, no VLAs.
  Use `__builtin_load()` / `__builtin_store()` for MMIO.
- **No floats** — all image processing is integer-only. No 64-bit
  arithmetic (B32P3 is 32-bit).
- **Deferred I2C** — by convention, write OV7670 registers with
  camera capture stopped for clean frame boundaries. Use the pattern:
  `cam_disable()` → I2C → `cam_enable_phase(1)`. This is a design
  choice, not a hardware requirement.
- **DMA alignment** — all DMA transfers must be 32-byte aligned.
  Call `cache_flush_data()` before MEM→device and after device→MEM.
- **VRAMPX is write-only** for CPU — cannot read back pixel data.
- Timer 1 is used for USB HID polling (10 ms). Timer 2 is `delay()`.
- USB keyboard input via CH376 on `FPGC_SPI_USB_1` (bottom port).
- Debug output via `uart_print_str()` / `uart_print_hex()`.

## Key design decisions & lessons

1. **DMA integration over sub-arbiter:** V1 used a CameraSubArbiter
   to write camera data directly to SDRAM. It crashed the system due
   to FPGA routing/placement issues (not logic bugs — testbenches
   passed). V2 routes all camera data through the proven DMA engine
   SDRAM write path. No crashes since.

2. **Hardware dither/LUT in DMA:** The DMA engine applies
   auto-contrast LUT, ordered dithering, and 2× upscaling inline
   during CAM2VRAM transfers. Zero CPU cost for live preview
   processing.

3. **Streaming capture (no double buffering):** `CAM2VRAM` streams
   sensor pixels directly into VRAMPX via the DMA handshake — there
   is no intermediate full-frame SDRAM buffer. The `current_buf`
   toggle in CameraCapture.v is metadata only (used by `CAM2MEM` to
   pick a buffer address). This is intentional: it avoids frame
   latency at low FPS settings and produces a visible rolling
   shutter effect that gives immediate visual feedback of the
   sensor's exposure. For still captures, `CAM2MEM` writes a
   complete frame to SDRAM before processing.

4. **Deferred I2C pattern:** All OV7670 register writes are deferred
   to the gap between DMA completion and next frame start. This is a
   design convention for clean frame boundaries, not a hardware
   requirement — I2C works with capture running, but batching writes
   between frames is cleaner.

5. **SPI display for portability:** The Cyclone 10 design replaces
   HDMI with an ILI9341 SPI TFT. Software is unaffected — same
   VRAMPX addresses, same palette writes. Display controller handles
   palette lookup + RGB565 conversion in hardware.
