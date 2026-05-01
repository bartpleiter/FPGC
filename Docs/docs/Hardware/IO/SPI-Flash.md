# SPI Flash

The FPGC carries two **Winbond W25Q128JV** SPI flash chips, each
storing 128 Mbit (16 MiB). They are the sole non-volatile storage on
the board and are partitioned by role rather than by software:

- **Flash 1** holds the FPGA bitstream, the bootloader, and the
  factory image of BDOS. The CPU reaches it through the `SPI0`
  controller.
- **Flash 2** holds the BRFS filesystem image. The CPU reaches it
  through the `SPI1` controller. Read-heavy traffic from BRFS goes
  through this chip every boot, so it sits behind a Quad-IO read
  path (see [Read modes](#read-modes) below).

The chips share the same protocol and the same physical interface;
only the controller wiring on the FPGA side differs.

## Bus shape

Both chips are mounted in 8-pin SOIC packages with the four signals
of standard SPI plus two side-band pins that, on Quad-IO devices,
can be repurposed as additional data lines.

| Pin    | Single-bit SPI role | Quad-IO role |
|--------|---------------------|--------------|
| `CS`   | chip select         | chip select  |
| `CLK`  | serial clock        | serial clock |
| `DI`   | host → device       | `IO0` (bidir)|
| `DO`   | device → host       | `IO1` (bidir)|
| `WP`   | write-protect       | `IO2` (bidir)|
| `HOLD` | freeze transaction  | `IO3` (bidir)|

In single-bit SPI mode `WP` and `HOLD` are tied high and `IO0..3`
collapse to the familiar `MOSI` / `MISO` pair. In Quad-IO mode all
four data lines turn around together and the host clocks four bits
per `CLK` edge instead of one.

Both chips run from 3.3 V. The supply is shared with the rest of the
3.3 V rail on the PCB; no level shifters are needed.

## Address space and erase granularity

The W25Q128JV is organised as:

| Granularity     | Size      | Purpose                          |
|-----------------|-----------|----------------------------------|
| Page            | 256 B     | Smallest **write** unit          |
| Sector          | 4 KiB     | Smallest **erase** unit          |
| Block           | 32 / 64 KiB | Larger erase unit (faster)     |
| Chip            | 16 MiB    | Whole-chip erase                 |

NOR flash semantics apply: a page program can only flip bits from
`1` to `0`. Bits go back to `1` only via an erase, and erases work
on whole sectors at a time. Software is responsible for
read-modify-write whenever an in-place update straddles non-erased
data.

24-bit addressing covers the full 16 MiB. The chip also supports
4-byte address mode for >16 MiB devices, but the FPGC never enables
it.

## Command set

The commands the FPGC actually issues are a small subset of the
W25Q128JV instruction set. Each command frame is one opcode byte
followed (for the non-status commands) by a 24-bit address and any
data:

| Opcode  | Name              | Direction        | Purpose                              |
|---------|-------------------|------------------|--------------------------------------|
| `0x03`  | Read Data         | flash → host     | Single-bit read at any address       |
| `0xEB`  | Quad-IO Fast Read | flash → host     | Quad-IO read with continuous mode    |
| `0x06`  | Write Enable      | host → flash     | Arms the next program/erase          |
| `0x05`  | Read Status Reg 1 | flash → host     | Returns BUSY (bit 0) and WEL (bit 1) |
| `0x02`  | Page Program      | host → flash     | Writes up to 256 B inside one page   |
| `0x20`  | Sector Erase      | host → flash     | Erases a 4 KiB sector                |
| `0x9F`  | JEDEC ID          | flash → host     | Returns the manufacturer/device ID   |

Every program or erase must be preceded by Write Enable and followed
by polling Status Register 1 until BUSY clears. Sector erase takes
on the order of 50 ms on this part; page program on the order of
1 ms. Reads do not need any wait state — the chip streams as fast as
the host clocks.

## Read modes

### Single-bit reads (`0x03`)

The classic SPI read: opcode + 24-bit address + N data bytes,
clocked one bit per `CLK`. Used by Flash 1 (boot and factory image)
because the bootloader has no reason to bring up Quad-IO before
reaching BDOS.

### Quad-IO Fast Read (`0xEB`) with continuous mode

Used exclusively by Flash 2 (BRFS). The host sends:

- the `0xEB` opcode on `IO0` (one bit per cycle),
- the 24-bit address spread across `IO0..3` (four bits per cycle —
  6 cycles total),
- an 8-bit "mode" byte spread across `IO0..3` (2 cycles),
- four dummy cycles to turn the bus around,

after which the chip clocks data back across all four `IO` lines at
four bits per cycle. The end-to-end win is roughly 4× over the
single-bit path for everything except the small per-transaction
header.

The mode byte serves a second purpose: if its top nibble is `0xA`
the chip enters **continuous read mode**, where the next transaction
is allowed to skip the opcode entirely and start straight from the
address phase. The FPGC does not currently use continuous mode (the
mode byte is sent as `0x00` so the next transaction always starts
fresh) — leaving it off keeps the boot path and the BDOS reset path
deterministic, at the cost of a few extra cycles per BRFS read.

The Quad-IO data path is driven by the dedicated `QSPIflash`
controller. It speaks single-bit SPI for everything that is not a
Fast Read (status polling, page program, sector erase, JEDEC ID),
which means status-bound operations like erasing a sector still use
the same opcodes and timing as the rest of the system.

## DMA path

Both controllers are wired into the [DMA engine](../DMA.md)'s SPI
burst port. Software still issues the opcode and address bytes
through the per-byte CPU MMIO interface — the DMA engine takes over
only for the data phase.

For Flash 1 the engine streams bytes one per `CLK`, in either
direction, using the `MEM2SPI` and `SPI2MEM` modes.

For Flash 2 the engine has a third mode, `SPI2MEM_QSPI`, that hands
the address byte and the start of the read directly to `QSPIflash`
and then drains four bits per `CLK` from `IO0..3`. From the
software's point of view the only differences from `SPI2MEM` are the
mode bits in `DMA_CTRL` and an extra register that holds the 24-bit
flash address.

The DMA path is what makes BRFS reads fast enough to mount the
filesystem in well under a second on cold boot.

## Boot flow

The Cyclone IV FPGA is configured from a separate dedicated
configuration flash (not one of the two W25Q chips above). After
configuration finishes, the CPU comes out of reset at the bootloader
entry point in on-chip ROM, which then walks Flash 1 to load BDOS
into SDRAM. Flash 2 is untouched until BDOS calls `brfs_mount` and
the BRFS layer starts reading the superblock through `SPI1`.

## Wear and reliability

The W25Q128JV is rated for 100 000 program/erase cycles per sector
and 20 years data retention. The FPGC has no wear-levelling: BRFS
overwrites a fixed superblock on every `sync`, so the superblock
sectors are the first thing that will eventually wear out. In
practice this is far beyond any realistic developer workload, but
keep it in mind if you build a BRFS-resident logger that calls
`sync` from a tight loop.
