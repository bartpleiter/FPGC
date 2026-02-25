# CPU Memory System

The B32P3 accesses memory through a two-level hierarchy: fast on-chip caches backed by external SDRAM. All non-SDRAM memories (ROM, VRAM, I/O) are accessed directly without caching. This page covers the L1 caches, the cache controller, the SDRAM interface, and how I/O peripherals are accessed.

## Memory Hierarchy Overview

The CPU sees a flat 27-bit address space. Different address ranges route to different hardware:

| Address Range | Target | Access Path | Typical Latency |
|---|---|---|---|
| `0x0000000` - `0x6FFFFFF` | SDRAM (64 MiW) | L1I/L1D cache | 1 cycle (hit) |
| `0x7000000` - `0x700001B` | I/O peripherals | Memory Unit | Variable (SPI, UART) |
| `0x7800000` - `0x78003FF` | ROM (1 KiW) | Direct BRAM | 1 cycle |
| `0x7900000` - `0x790041F` | VRAM32 | Direct BRAM | 1 cycle |
| `0x7A00000` - `0x7A02001` | VRAM8 | Direct BRAM | 1 cycle |
| `0x7B00000` - `0x7B12BFF` | VRAMpixel | External SRAM + FIFO | 1 cycle (writes buffered) |
| `0x7C00000` - `0x7C00001` | CPU internal I/O | Direct registers | Immediate |

SDRAM reads and instruction fetches go through L1 caches. Everything else bypasses the caches entirely.

## L1 Caches

There are two separate L1 caches: L1I for instructions and L1D for data. They share the same physical structure and cache controller but serve different pipeline stages.

### Cache Line Format

Each cache has 128 lines. A single cache line is 271 bits wide:

| Field | Bits | Description |
|---|---|---|
| Data | [270:15] | 256 bits = 8 words of 32 bits |
| Tag | [14:1] | 14-bit tag for address matching |
| Valid | [0] | 1 if the line contains valid data |

The 24-bit SDRAM word address is split as follows:

```text
[23:10]  14-bit tag
[9:3]     7-bit index  (selects 1 of 128 cache lines)
[2:0]     3-bit offset (selects 1 of 8 words within the line)
```

This gives a direct-mapped cache with 128 lines of 8 words each, covering 1024 words of data per cache. The 14-bit tag supports the full 64 MiW SDRAM address space.

### Storage

Each cache is implemented as a dual-port block RAM (DPRAM) on the FPGA. Both ports run at 100 MHz:

- **Pipeline port**: Read-only from the CPU's perspective. The pipeline drives the index to look up a cache line and checks the tag for a hit.
- **Controller port**: Read/write access for the cache controller, used during fills and evictions.

The dual-port design lets the pipeline check for hits on one port while the controller simultaneously fills or evicts on the other, without stalling each other.

### L1I (Instruction Cache)

The instruction cache serves the IF stage. On each cycle, IF drives the L1I pipeline port with the current PC's cache index. The tag and valid bit are checked against the PC:

- **Hit**: The instruction word is extracted from the cache line using the 3-bit offset. No stall.
- **Miss**: `cache_stall_if` fires, freezing the entire pipeline. The cache controller fetches the line from SDRAM. When it finishes, IF selects the result directly from the controller output (not the DPRAM, which takes one more cycle to reflect the write).

L1I is read-only from the cache controller's perspective. No dirty tracking or write-back is needed.

### L1D (Data Cache)

The data cache serves the MEM stage for SDRAM loads and stores. It uses a **write-back, write-allocate** policy:

- **Read hit**: Data comes directly from the cache line. No stall (after the initial 1-cycle DPRAM read latency).
- **Read miss**: The cache controller fetches the line from SDRAM. If the existing line is dirty, it's written back first.
- **Write hit**: The word is updated in the cache line, and the line is marked dirty. No SDRAM access needed.
- **Write miss**: The cache controller fetches the target line from SDRAM (evicting the previous line if dirty), then modifies the requested word and marks the line dirty.

There's a timing subtlety with L1D hits: the DPRAM has 1-cycle read latency, so when a new instruction first arrives in MEM, the cache output isn't valid yet. The pipeline waits one extra cycle before checking the tag. This means every SDRAM access takes at least 2 cycles in MEM, even on a cache hit.

### Dirty Bit Optimization

The dirty bit for each of the 128 L1D lines is stored in a separate register array, not inside the DPRAM. This matters because checking the DPRAM for a dirty line would require reading it first (1-cycle latency). With the dirty bits available combinationally, the cache controller can skip the DPRAM read entirely when the line is clean. On a clean-line read miss, this saves 2 cycles by jumping straight to the SDRAM fetch.

## Cache Controller

A single cache controller manages both L1I and L1D. It runs at 100 MHz and handles miss processing, dirty line eviction, write-back, and prefetching through a 29-state FSM.

### Request Priority

When multiple requests arrive simultaneously, the controller uses a fixed priority:

1. **Cache clear** (highest): Triggered by the `ccache` instruction
2. **L1D requests**: Data loads and stores from MEM stage
3. **L1I requests**: Instruction fetch misses from IF stage
4. **Prefetch** (lowest): Speculative L1I prefetch during idle time

Data requests get priority over instruction requests because data misses are harder to hide. An instruction miss stalls the front of the pipeline, but work may still be in-flight in later stages. A data miss stalls the back of the pipeline, blocking everything.

### L1D Read Miss Path

When MEM has a cache miss on a load:

1. Check the dirty bit register for the target cache index (combinational, no wait).
2. **Clean line (fast path)**: Issue SDRAM read immediately. When data arrives, write the new line to the DPRAM and clear the dirty bit.
3. **Dirty line (slow path)**: Read the old line from DPRAM (1 cycle), write it back to SDRAM, then issue the read for the new line.

The fast path avoids reading the old cache line from DPRAM entirely, since we know it's clean and can be safely overwritten.

### L1D Write Path

On a store to SDRAM:

1. Read the target cache line from DPRAM (1 cycle wait).
2. **Hit**: Modify the target word in the existing line, write it back to DPRAM, set the dirty bit. Done.
3. **Miss**: Evict the current line if dirty (write-back to SDRAM), fetch the target line from SDRAM, modify the word, write to DPRAM, set dirty bit.

Write-allocate means we always bring the full cache line into the cache before modifying it. This keeps things simple and ensures subsequent reads to nearby addresses will hit.

### L1I Prefetching

After servicing an L1I miss, the controller queues a prefetch for the next cache line (`address + 8`). This exploits the fact that instruction fetches are usually sequential, so the next line will likely be needed soon.

Prefetches only execute during true idle time. If any real CPU request arrives while a prefetch is in progress, the prefetch is immediately cancelled. This ensures prefetching never delays real work.

### Cache Clear

The `ccache` instruction triggers a full clear of both caches. The controller walks all 128 L1D lines, writing back any dirty ones to SDRAM, then zeros out both L1I and L1D. This is used before transferring control to newly loaded code (like a bootloader loading a program) to ensure the caches don't serve stale data.

The dirty bit register makes this efficient: clean lines are skipped without reading the DPRAM.

### Pipeline Flush Interaction

If a pipeline flush occurs while an L1I fetch is in-flight (for example, a branch redirects the PC while the controller is waiting for SDRAM), the fetched data is still written to the cache, but the result is not forwarded to the pipeline. This avoids wasting the SDRAM access, and the line will be in the cache for the next access to that address.

## SDRAM Controller

The SDRAM controller provides a simple burst interface to the cache controller. Every operation reads or writes exactly one cache line (256 bits = 8 words). There is no sub-line access.

### Hardware

The FPGA connects to two W9825G6KH-6 SDRAM chips wired as a 32-bit-wide bus. Each chip provides 16 data bits, giving 32 bits total per cycle. Together they provide 64 MiB of storage.

The SDRAM runs at 100 MHz with a 180-degree phase-shifted clock. This phase shift ensures the FPGA samples data at the center of the SDRAM's valid data window, maximizing setup and hold margins.

### Interface

The cache controller sends a 21-bit line address, an optional 256-bit write data word, and a start signal. The SDRAM controller handles all the JEDEC timing (row activation, CAS latency, precharge, refresh) internally and signals completion via `done`.

### Burst Timing

| Operation | Cycles | Notes |
|---|---|---|
| Row activation | 2 | $t_{RCD}$: activate to read/write delay |
| Read burst | 8 + 2 | 8 data words + 2 CAS latency cycles |
| Write burst | 8 + 4 | 8 data words + $t_{WR} + t_{RP}$ guard time |
| Auto-refresh | 6 | $t_{RFC}$ recovery time |

Both read and write use auto-precharge (A10 set during the command), so the controller doesn't need an explicit precharge state. Refresh happens every 782 cycles (well within the JEDEC 64ms/8192 requirement).

A complete cache miss (with no dirty eviction) takes roughly 12 cycles: 2 for activation + 10 for the read burst. A dirty eviction adds another ~14 cycles for the write-back. With both, a worst-case cache miss is around 26 cycles.

## I/O Access (Memory Unit)

Addresses in the range `0x7000000` to `0x77FFFFF` route to the Memory Unit, which provides a simple request/done interface to slow peripherals. The pipeline stalls until the Memory Unit signals completion.

### Peripherals

| Address | Peripheral | Notes |
|---|---|---|
| `0x7000000` | UART TX | Write a byte |
| `0x7000001` | UART RX | Read received byte |
| `0x7000002` - `0x7000007` | Timers 1-3 | Value and trigger registers |
| `0x7000008` - `0x7000016` | SPI 0-5 | Data and chip-select for Flash, USB, Ethernet, SD |
| `0x7000019` | Boot mode | Read: hardware boot switch |
| `0x700001A` | Microsecond counter | Read: free-running counter |
| `0x700001B` | User LED | Write: control LED |

The Memory Unit instantiates all the SPI masters, UART controllers, and timer modules internally. SPI transfers run at either 25 MHz or 12.5 MHz depending on the peripheral (Flash and SD use 25 MHz, USB and Ethernet use 12.5 MHz).

Each I/O access stalls the pipeline for the duration of the peripheral operation. For SPI, this is roughly 16 cycles per byte (8 bits at half the clock rate). For UART, it depends on the baud rate. These are intentionally simple peripherals. There is no DMA or interrupt-driven transfer; the CPU busy-waits for each byte.
