# DMA Engine

The FPGC includes a single-channel DMA controller, `DMAengine`, that
moves data between SDRAM, the SPI flash/SD/Ethernet bursts, and the
pixel framebuffer (VRAMPX) without involving the CPU pipeline.

It is implemented in
`Hardware/FPGA/Verilog/Modules/IO/DMAengine.v` and instantiated from
the top-level `FPGC.v` next to the SDRAM controller and the
`MemoryUnit`.

## Why

Three workloads dominated CPU cycles before the DMA existed:

1. **Disk I/O** — every BRFS sector read/write (512 bytes) was a tight
   per-byte SPI loop that stalled the CPU for milliseconds.
2. **Ethernet packet RX/TX** — copying packet payloads between the
   ENC28J60 SRAM and SDRAM through SPI4.
3. **Framebuffer presents** — full-frame `memcpy` of 76,800 bytes
   into VRAMPX, which costs ~18 ms via plain CPU stores and produced
   visible tearing because the GPU scans VRAMPX continuously.

The DMA engine offloads all three to dedicated hardware.

## Register Block

The engine exposes a 5-register MMIO block in the I/O region. See
[Memory Map](Memory-Map.md) for the absolute addresses.

| Offset | Name        | R/W | Purpose                                                     |
|--------|-------------|-----|-------------------------------------------------------------|
| `0x70` | `DMA_SRC`   | R/W | Source byte address (SDRAM for MEM2*, SPI for SPI2MEM)      |
| `0x74` | `DMA_DST`   | R/W | Destination byte address                                    |
| `0x78` | `DMA_COUNT` | R/W | Byte count (must be > 0 and a multiple of 32)               |
| `0x7C` | `DMA_CTRL`  | R/W | Mode + flags + start (bit `[31]` is W1S start, self-clears) |
| `0x80` | `DMA_STATUS`| R   | `{29'd0, sticky_error, sticky_done, busy}`                  |

`DMA_CTRL` layout:

| Bits   | Field              | Meaning                                               |
|--------|--------------------|-------------------------------------------------------|
| `3:0`  | `MODE`             | 0 = MEM2MEM, 1 = MEM2SPI, 2 = SPI2MEM, 3 = MEM2VRAM   |
| `4`    | `IRQ_EN`           | Raise interrupt 7 when the transfer completes         |
| `7:5`  | `SPI_ID`           | SPI peripheral ID for SPI modes                       |
| `31`   | `START` (W1S)      | Writing 1 latches the registers and starts the engine |

`DMA_STATUS` bits:

- `busy` — high while the engine is transferring.
- `done` — sticky; set when a transfer finishes successfully.
- `error` — sticky; set on alignment violation or count == 0.

The sticky bits are cleared on the rising edge of a status read and
when a new transfer is started.

## Alignment Rules

Every transfer **must** satisfy:

- `DMA_SRC % 32 == 0`
- `DMA_DST % 32 == 0`
- `DMA_COUNT % 32 == 0` and `DMA_COUNT > 0`

For MEM2VRAM, `DMA_DST` must additionally lie inside the VRAMPX
decode window (`0x1EC00000 .. 0x1EC1FFFF`), and
`DMA_DST + DMA_COUNT` must not exceed it.

Misaligned values are rejected: the engine immediately enters the
`ERROR` state, sets `STATUS.error`, and never touches memory.

## Cache Coherency

The DMA engine reads SDRAM through the `SDRAMarbiter`'s DMA port,
which bypasses the CPU's L1 data cache. **Software is responsible
for cache coherency.** Two rules:

- **Producer side (CPU writes the source, DMA reads it):**
  invalidate/flush the L1d cache (`ccached`) before issuing the
  transfer, so any dirty lines are written back to SDRAM.
- **Consumer side (DMA writes the destination, CPU reads it):**
  invalidate the L1d cache (`ccached`) after the transfer, so stale
  cached lines do not shadow the new data.

The libfpgc and userlib helpers (see below) bracket their synchronous
transfers with `ccached` automatically. Asynchronous helpers leave
this to the caller.

VRAMPX is not cached on the CPU side, so MEM2VRAM only needs the
producer-side flush.

## Modes

### MEM2MEM

Plain SDRAM-to-SDRAM copy, in 32-byte cache-line bursts. Used as the
fast `memcpy` primitive for large buffers.

### MEM2SPI / SPI2MEM

Streams between SDRAM and a SPI peripheral via the SPI burst port,
which feeds an internal TX/RX FIFO and drives `SimpleSPI2`. Used by
the BRFS sector layer (SPI flash) and the Ethernet driver
(ENC28J60 packet RX/TX).

The selected SPI peripheral is chosen by `SPI_ID` in `DMA_CTRL` and
must already be selected (`CS` low) by the driver before starting
the transfer.

**SPI flash writes (page-program) are not DMA-accelerated on
SPI1 (QSPIflash).** The QSPIflash controller's 1-bit SPI burst
path does not reliably handle the DMA engine's per-32-byte
`dma_select` cycling between SDRAM reads and SPI pushes.
`spi_flash_write_words` falls back to byte-by-byte `spi_transfer`
for SPI1; this is not a bottleneck because page-program latency is
dominated by the flash chip's internal program cycle (~1 ms), not
bus bandwidth. SPI flash **reads** on SPI1 use the dedicated QSPI
Fast Read DMA path (`SPI2MEM_QSPI` mode), which issues a single
continuous burst without per-chunk select cycling.

DMA MEM2SPI is used for SPI0 (Flash 0) and SPI4 (ENC28J60), which
are `SimpleSPI2` instances that handle the per-chunk cycling
correctly.

### MEM2VRAM

Streams a 32-byte-aligned region of SDRAM into the VRAMPX
write-port FIFO. The engine paces itself against the FIFO's `full`
flag, so it cannot overrun the framebuffer SRAM. This is the
primitive used for tear-free full-frame presents: software composes
a frame in an SDRAM back buffer and blits it in one shot.

## Interrupt

When `DMA_CTRL.IRQ_EN` is set, the engine raises **interrupt line 7**
on completion (success or error). The handler should read
`DMA_STATUS` to clear the sticky bits. See the
[Interrupt Assignments](CPU/CPU.md#interrupt-assignments) table.

## C API

Both `libfpgc` (used by BDOS) and `userlib` (used by userBDOS
programs) ship a `dma.h` with the same surface. The synchronous
helpers are the easy path:

```c
#include <dma.h>

/* Synchronous SDRAM-to-SDRAM copy; brackets with ccached on both
 * sides. Returns 0 on success, -1 on engine error. */
int dma_copy(unsigned int dst, unsigned int src, unsigned int count);

/* Synchronous SDRAM-to-VRAMPX blit. dst must be in 0x1EC00000..0x1EC20000,
 * src 32-byte aligned in SDRAM, count a multiple of 32. Flushes the L1d
 * cache before the transfer; no post-invalidate needed (VRAMPX is
 * write-only from the CPU side). */
int dma_blit_to_vram(unsigned int dst, unsigned int src, unsigned int count);
```

For overlap with CPU work there are async equivalents:

```c
void dma_start_mem2mem (unsigned int dst, unsigned int src, unsigned int count);
void dma_start_mem2vram(unsigned int dst, unsigned int src, unsigned int count);

int          dma_busy(void);    /* non-zero while STATUS.busy == 1 */
unsigned int dma_status(void);  /* one read; clears sticky bits     */

void cache_flush_data(void);    /* `ccached` wrapper                */
```

The async path leaves cache management to the caller — call
`cache_flush_data()` before starting if the CPU just wrote the
source, and after polling `dma_busy() == 0` if the CPU is about to
read the destination.

### Typical pattern: tear-free framebuffer present

```c
#include <syscall.h>
#include <dma.h>

#define PIXEL_FB_ADDR  0x1EC00000
#define W              320
#define H              240

unsigned int back_buf;   /* 32-byte aligned, holds a full frame */

int main(void) {
    unsigned char *raw = (unsigned char *)sys_heap_alloc(W * H + 32);
    back_buf = ((unsigned int)raw + 31u) & ~31u;

    while (running) {
        render_into(back_buf);                   /* CPU writes SDRAM */
        dma_blit_to_vram(PIXEL_FB_ADDR,
                         back_buf,
                         (unsigned int)(W * H)); /* atomic present  */
    }
}
```
