# FPGC Project Context

Orientation file for AI coding agents (GitHub Copilot, etc.) working
on the FPGC repo. Optimised for token efficiency — tables over prose,
exact paths and make targets throughout.

## What FPGC is

FPGC ("FPGA Computer") is a from-scratch computer built around a
custom 32-bit RISC CPU on an FPGA. The repo contains everything from
Verilog RTL through the C toolchain to the operating system and user
programs. Doom runs on it.

- **CPU**: B32P3 — 5-stage pipeline, byte-addressable, 16 GP
  registers, hardware multiply/divide, optional FP64 coprocessor.
- **GPU**: tile engine (BG + window planes) + 320×240 8-bit pixel
  framebuffer, all VRAM-mapped.
- **Storage**: 64 MiB SDRAM (cached), SPI flash, microSD card. BRFS
  v2 filesystem on both flash (`/`) and SD card (`/sdcard`).
- **I/O**: 6 SPI buses, 3 timers, UART, DMA engine (7 modes),
  CH376 USB host (keyboard), ENC28J60 Ethernet.
- **OS**: BDOS — single-user, cooperative multitasking, with
  Bourne-style shell, VFS, process model (up to 16 processes),
  USB keyboard, Ethernet.
- **Toolchain**: cproc (C11) → QBE (SSA optimiser) → ASMPY (Python
  assembler/linker). Self-hosting on BDOS is operational.

## Repo layout

```
Hardware/
  FPGA/                  Verilog RTL (CPU, GPU, IO, Memory modules)
  PCB/                   KiCad PCB design
BuildTools/
  cproc/                 C11 frontend (emits QBE IR)
  QBE/                   SSA optimiser + B32P3 backend
  ASMPY/                 Python assembler with linker
Software/
  ASM/                   crt0 startup files, bootloaders, raw asm programs
  C/
    libc/                picolibc-derived freestanding C library
    libfpgc/             hardware drivers, libterm, BRFS, DMA, SD, CH376
    kernel/              BDOS kernel sources (v4)
    userlib/             userland syscall wrappers + DMA + fixedmath
    userBDOS/            user programs (Doom, editor, shell, snake, etc.)
    bareMetal/           bare-metal hardware test programs
Tests/
  CPU/                   Verilog testbenches (14 categories incl. DMA)
  C/                     Compiler end-to-end tests (~20 categories)
  host/                  Host-side C unit tests (libterm)
  asm-link/              Assembler/linker regression tests
Scripts/
  BCC/                   compile_modern_c.sh (main build tool), cc.sh
  ASM/                   Bootloader compilation
  Programmer/            UART upload, SPI flash, FNP network tools
  BDOS/                  SD card host tools (sd_read_brfs.py, sd_write_brfs.py)
  Tests/                 Test runners, host test scripts
  Simulation/            CPU/SDRAM simulation helpers
  Graphics/              Texture conversion
Docs/
  docs/                  MkDocs site (canonical user-facing docs)
  plans/                 Design plans for in-flight and completed work
  context/               This file + BDOS-context.md
Files/
  BRFS-init/             Staged filesystem image (bin/, lib/, etc.)
```

## CPU (B32P3)

5-stage pipeline (IF/ID/EX/MEM/WB). Byte-addressable (`char`=1,
`short`=2, `int`=4, pointer=4). 32-bit address space.

16 GP registers: `r0`=zero, `r1`=return, `r4`–`r7`=args,
`r13`=SP, `r14`=FP, `r15`=RA.

### Physical address map

| Region | Address range | Size | Notes |
|--------|---------------|------|-------|
| SDRAM | `0x00000000`–`0x03FFFFFF` | 64 MiB | L1 I/D cached |
| MMIO | `0x1C000000`–`0x1C000084` | 132 B | Peripheral registers |
| ROM | `0x1E000000`–`0x1E000FFF` | 4 KiB | CPU reset vector |
| VRAM pattern+palette | `0x1E400000`–`0x1E401FFF` | | Tile char/palette data |
| VRAM tiles | `0x1E800000`–`0x1E808007` | | BG + window tile/color tables |
| VRAM pixels | `0x1EC00000`–`0x1EC7FFFF` | | 320×240 pixel FB + palette |
| PC/HW stack | `0x1F000000`–`0x1F000007` | | PC backup, HW stack ptr |

ISA reference: [Docs/docs/Hardware/CPU/CPU.md](../docs/Hardware/CPU/CPU.md).

### SDRAM memory layout (used by BDOS)

| Region | Range | Size |
|--------|-------|------|
| Kernel code + BSS | `0x000000`–`0x0FFFFF` | 1 MiB |
| Kernel stacks (3) | `0x100000`–`0x10FFFF` | 64 KiB |
| Kernel heap | `0x110000`–`0x1FFFFF` | ~960 KiB |
| Process memory pool | `0x200000`–`0x1FFFFFF` | 30 MiB |
| BRFS SD cache | `0x2000000`–`0x23FFFFF` | 4 MiB (LRU) |
| BRFS SPI flash cache | `0x2400000`–`0x3FFFFFF` | 28 MiB |

Stacks: main `0x107FFC`, syscall `0x10BFFC`, interrupt `0x10FFFC`.

## MMIO peripheral map

All at base `0x1C000000`. cproc has no `volatile` — use
`__builtin_load(addr)` / `__builtin_store(addr, val)` (also `loadb`,
`storeb`, `loadh`, `storeh` variants).

| Offset | Register | Notes |
|--------|----------|-------|
| `0x00` | UART TX | |
| `0x04` | UART RX | |
| `0x08`–`0x0C` | Timer 0 val/ctrl | Free (deferred ENC28J60 retry) |
| `0x10`–`0x14` | Timer 1 val/ctrl | USB keyboard HID polling (10 ms) |
| `0x18`–`0x1C` | Timer 2 val/ctrl | `delay()` function |
| `0x20`–`0x24` | SPI0 data/CS | SPI Flash 0 (BRFS boot) |
| `0x28`–`0x2C` | SPI1 data/CS | SPI Flash 1 |
| `0x30`–`0x34` | SPI2 data/CS | CH376 USB host (top) |
| `0x38` | CH376 top INT# | Active-low interrupt pin (read-only) |
| `0x3C`–`0x40` | SPI3 data/CS | CH376 USB host (bottom) |
| `0x44` | CH376 bottom INT# | Active-low interrupt pin (read-only) |
| `0x48`–`0x4C` | SPI4 data/CS | ENC28J60 Ethernet |
| `0x54`–`0x58` | SPI5 data/CS | SD card |
| `0x64` | Boot mode | 0=UART, 1=SPI flash |
| `0x68` | Microsecond counter | Free-running µs timer |
| `0x6C` | User LED | |
| `0x70` | DMA SRC | Source address |
| `0x74` | DMA DST | Destination address |
| `0x78` | DMA COUNT | Transfer size (bytes, 32-aligned) |
| `0x7C` | DMA CTRL | Mode[3:0], IRQ_EN[4], SPI_ID[7:5], START[31] |
| `0x80` | DMA STATUS | BUSY[0], DONE[1], ERROR[2] (sticky, clear-on-read) |
| `0x84` | DMA QSPI ADDR | Flash address for QSPI reads |

### SPI bus assignments

| Bus | ID | Device | Constant |
|-----|-----|--------|----------|
| SPI0 | 0 | SPI Flash 0 (BRFS boot) | `FPGC_SPI_FLASH_0` |
| SPI1 | 1 | SPI Flash 1 | `FPGC_SPI_FLASH_1` |
| SPI2 | 2 | CH376 USB (top) | `FPGC_SPI_USB_0` |
| SPI3 | 3 | CH376 USB (bottom) | `FPGC_SPI_USB_1` |
| SPI4 | 4 | ENC28J60 Ethernet | `FPGC_SPI_ETH` |
| SPI5 | 5 | SD card | `FPGC_SPI_SD_CARD` |

### Interrupt IDs

| ID | Source | BDOS handler |
|----|--------|-------------|
| 1 | UART RX | Ring buffer fill |
| 2 | Timer 0 | Deferred ENC28J60 ISR retry |
| 3 | Timer 1 | USB keyboard HID report polling |
| 4 | Timer 2 | `delay()` completion |
| 5 | Frame drawn | (unused) |
| 6 | ENC28J60 RX | Drain HW RX into kernel ring |
| 7 | DMA complete | Transfer done notification |

## DMA engine

6 registers at `0x1C000070`–`0x1C000084`. 7 transfer modes:

| Mode | Name | Direction |
|------|------|-----------|
| 0 | `MEM2MEM` | SDRAM → SDRAM |
| 1 | `MEM2SPI` | SDRAM → SPI device |
| 2 | `SPI2MEM` | SPI device → SDRAM |
| 3 | `MEM2VRAM` | SDRAM → VRAM (GPU bulk write) |
| 4 | `MEM2IO` | SDRAM → I/O |
| 5 | `IO2MEM` | I/O → SDRAM |
| 6 | `SPI2MEM_QSPI` | QSPI flash → SDRAM (quad-SPI fast read) |

All transfers must be **32-byte aligned** (cache-line size). Call
`cache_flush_data()` (ccached instruction) before MEM→device and
after device→MEM transfers for coherency.

Kernel driver: `libfpgc/io/dma.c` + `dma_asm.asm`.
Userlib driver: `userlib/src/dma.c` + `dma_asm.asm` (used by Doom).
Hardware docs: [Docs/docs/Hardware/DMA.md](../docs/Hardware/DMA.md).

## C toolchain

```
src.c → cpp → cproc → QBE → b32p3 .asm → ASMPY linker → .list/.bin
```

- **cproc** (`BuildTools/cproc/`) — C11 frontend. No inline asm, no
  `volatile`. Use `__builtin_store{,b,h}` / `__builtin_load{,b,h}`
  for MMIO.
- **QBE** (`BuildTools/QBE/`) — SSA optimiser + B32P3 code generator.
- **ASMPY** (`BuildTools/ASMPY/`) — Python assembler with linker,
  data directives, pseudo-instructions (`load32`, `addr2reg`),
  reloc-table for PIC user programs.

Build orchestration: `Scripts/BCC/compile_modern_c.sh`, invoked by
the top-level Makefile.

### Build targets

| Target | What it builds |
|--------|---------------|
| `make compile-kernel` | BDOS kernel (~224 KB binary) |
| `make compile-userbdos file=<name>` | Single userBDOS program |
| `make compile-userbdos-all` | All userBDOS programs (~35) |
| `make compile-doom` | Doom port |
| `make compile-bootloader` | Two-phase bootloader (RAM → ROM) |
| `make compile-asm file=<name>` | Raw assembly program |
| `make compile-c-baremetal file=<name>` | Bare-metal C test program |

### Self-hosting targets

| Target | Purpose |
|--------|---------|
| `make selfhost-qbe` | Cross-compile QBE for on-device use |
| `make selfhost-cproc` | Cross-compile cproc for on-device use |
| `make selfhost-all` | Both of the above |
| `make stage-cc-toolchain` | Stage complete on-device C toolchain in `Files/BRFS-init/` |

## BRFS v2 (filesystem)

Byte-native API, FAT-based with per-file linked chains.
Two storage backends via `brfs_storage_t` vtable:

| Backend | Mount point | Cache | Driver |
|---------|------------|-------|--------|
| SPI flash | `/` (root) | 28 MiB direct-mapped | `brfs_storage_spi_flash.c` |
| SD card | `/sdcard` | 4 MiB LRU | `brfs_storage_sdcard.c` |

- LE on disk; magic `BRF2`; superblock version `2`.
- Persistence is explicit (`brfs_sync()`).
- `fs_for_path()` routes paths starting with `/sdcard/` to the
  SD BRFS instance; everything else goes to SPI flash.
- See [BRFS docs](../docs/Software/BRFS.md).

## SD card

SPI-mode driver on bus 5 (`FPGC_SPI_SD_CARD`). SDHC/SDXC only
(block-addressed, 512-byte blocks). No card-detect switch — software
probes by attempting init.

- Driver: `libfpgc/io/sd.c` + `include/sd.h`
- BRFS wrapper: `libfpgc/fs/brfs_storage_sdcard.c`
- LRU cache: `libfpgc/fs/brfs_cache.c` (4 MiB at `0x2C00000`)
- Host tools: `make sd-read-brfs`, `make sd-write-brfs`
- Hardware docs: [Docs/docs/Hardware/IO/SD-Card.md](../docs/Hardware/IO/SD-Card.md)

## BDOS

Cooperative multitasking OS with process blocking and scheduling.
See [BDOS-context.md](BDOS-context.md) for the full source map,
syscall table, and subsystem detail.

Key facts for any change:

- Terminal I/O is ANSI-escape-driven on `fd 1`. Programs write
  `"\x1b[..."` via `sys_write(1, ...)`.
- Raw keyboard: open `/dev/tty` with `O_RAW[|O_NONBLOCK]`, `read`
  returns 4-byte LE event codes (ASCII or `KEY_*`).
- VFS exposes files, `/dev/tty`, `/dev/null`, `/dev/pixpal`,
  `/dev/uart`, `/dev/random`, and `/proc/*` under a unified
  `open`/`read`/`write`/`close`/`lseek`/`dup2` API.
- USB keyboard: connect/disconnect via INT# pin polling (main loop);
  HID report reading via timer ISR (10 ms).
- SD card mounted at `/sdcard` during boot if a card is present.
- Shell runs as `/bin/sh` (userland program), spawned by `/bin/init`.
- Processes block on sleep/waitpid; scheduler picks next READY process.
- Up to 16 processes with variable-size memory from a 30 MiB pool.

## User programs

Located in `Software/C/userBDOS/`. Build:
`make compile-userbdos file=<name>` (or `make compile-userbdos-all`).
Output: `Files/BRFS-init/bin/<name>`.

| Program | Description |
|---------|-------------|
| `init.c` | PID 1 — spawns `/bin/sh`, respawns on exit |
| `sh.c` | Bourne-style shell (variables, pipes, redirection, control flow) |
| `doom/` | Full Doom port (DMA-accelerated blitting) |
| `w3d.c` | Wolfenstein 3D-style raycaster |
| `edit.c` | Text editor (alt-screen, raw TTY) |
| `snake.c` | Snake game (non-blocking raw TTY) |
| `tetrisc.c` / `tetrish.c` | Tetris (client / host) |
| `mbrot.c` / `mbrotc.c` / `mbroth.c` | Mandelbrot (solo / cluster client / host) |
| `cmatrix.c` | CMatrix-style display |
| `tree.c` | Recursive directory listing |
| `bench.c` | Benchmark suite |
| `format.c` | SPI flash BRFS format utility |
| `sdformat.c` | SD card BRFS format utility |
| `asm-link.c` | On-device assembler/linker |
| `cpp.c` | On-device C preprocessor |
| `cat.c` / `ls.c` / `grep.c` / ... | External Unix-style utilities (pipe-compatible) |

Reference ports: `snake.c` (non-blocking raw TTY), `edit.c`
(blocking raw TTY, alt-screen, DECAWM).

## Development workflow (FNP)

Iterate on BDOS and user programs over Ethernet without reflashing:

| Target | What it does |
|--------|-------------|
| `make run-kernel` | Compile + upload kernel via UART |
| `make flash-kernel` | Flash kernel to SPI (persistent) |
| `make fnp-upload-userbdos file=<n>` | Compile + upload program to `/bin` over Ethernet |
| `make fnp-sync-files` | Sync `Files/BRFS-init/` to device filesystem |
| `make fnp-keyboard` | Interactive keyboard streaming to device |
| `make fnp-run cmd="<cmd>"` | Run shell command on device remotely |
| `make fnp-debug-userbdos file=<n>` | Compile, upload, run + capture UART debug output |
| `make run-userbdos file=<n>` | Compile, upload, and run (all-in-one) |
| `make run-doom` | Compile, upload, and run Doom |

FNP is a custom L2 protocol over ENC28J60 (EtherType `0xB4B4`).
Tool: `Scripts/Programmer/Network/fnp_tool.py`.

## Testing

| Target | What it runs |
|--------|-------------|
| `make check` | **Everything** (format-check + lint + all tests below) |
| `make test-cpu` | All CPU Verilog testbenches (parallel) |
| `make test-c` | All cproc+QBE+ASMPY compiler tests (parallel) |
| `make test-asmpy` | ASMPY Python unit tests |
| `make test-host` | All host-side C tests (libterm) |
| `make test-term` | libterm host unit tests |
| `make test-asm-link` | Assembler/linker regression tests |
| `make test-cpp` | C preprocessor regression tests |

Single-test: `make test-cpu-single file=…`, `make test-c-single file=…`.
Debug: `make debug-cpu file=…` (GTKWave waveform viewer).

When working on:

- **Verilog** → `make test-cpu` then `make test-c`
- **Toolchain** (cproc/QBE/ASMPY) → `make test-c`
- **Kernel / userBDOS** → `make compile-kernel` or `make compile-userbdos file=<n>`

## Simulation

| Target | Purpose |
|--------|---------|
| `make sim-cpu` | CPU simulation (Verilog) |
| `make sim-sdram` | SDRAM controller simulation |
| `make sim-bootloader` | Compile + simulate bootloader |
| `make quartus-timing` | Quartus FPGA timing analysis |

## Bare-metal test programs

`Software/C/bareMetal/` — hardware bring-up tests compiled with
`make compile-c-baremetal file=<name>`:

| Program | Tests |
|---------|-------|
| `spi1_dma_test.c` | SPI1 DMA transfers |
| `qspi_dma_test.c` | QSPI DMA reads |
| `sdcard_init_test.c` | SD card SPI init sequence |
| `sdcard_rw_test.c` | SD card read/write |
| `sdcard_multi_test.c` | SD card multi-block (CMD18/CMD25) |
| `sdcard_brfs_storage_test.c` | BRFS storage vtable over SD card |

Each has a dedicated `make compile-*` and `make run-*` target.

## Hardware (Verilog)

Source: `Hardware/FPGA/Verilog/Modules/`. Key modules:

- **CPU**: `B32P3.v`, `ALU.v`, `MultiCycleALU.v`, `Regbank.v`,
  `Stack.v`, `CacheControllerSDRAM.v`, `InterruptController.v`,
  `PipelineController.v`
- **GPU**: `BGWrenderer.v`, `PixelEngineSRAM.v`, `PixelPalette.v`,
  `TimingGenerator.v`, `HDMI/`
- **I/O**: `DMAengine.v`, `SimpleSPI.v`, `SimpleSPI2.v`,
  `QSPIflash.v`, `UARTrx.v`, `UARTtx.v`, `OStimer.v`,
  `MicrosCounter.v`
- **Memory**: `SDRAMcontroller.v`, `SDRAMarbiter.v`, `MemoryUnit.v`,
  `ROM.v`, `VRAM.v`

## House style

- C11 with cproc/QBE limits (no inline asm, no `volatile`). Use
  `__builtin_load*` / `__builtin_store*` for MMIO.
- Each `.c` is compiled independently. Standard `#include <header.h>`
  with `-I` paths from the build script.
- Assembly in dedicated `.asm` files — never inline.
- Keep `kernel/include/syscall_nums.h` and userlib `syscall.h` in
  sync. Removed syscalls stay reserved (return `-1`).

## Where to look next

- [Docs/docs/](../docs/) — MkDocs site, canonical user-facing docs.
- [Docs/plans/](../plans/) — design plans for past and in-flight work.
- [BDOS-context.md](BDOS-context.md) — deep dive into BDOS sources,
  syscalls, VFS, slots, HID, filesystem.
