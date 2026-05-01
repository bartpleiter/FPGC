# SD Card

The FPGC has a single microSD card slot wired to the FPGA as a
fifth SPI peripheral (`SPI5`). The slot accepts standard SDHC and
SDXC cards; legacy SDSC cards are not supported. The card is
optional — nothing in the boot path or in BDOS depends on a card
being inserted — but it is the second mass-storage option on the
board, alongside the on-board [SPI flash](SPI-Flash.md).

## Bus shape

The slot is wired in **SPI mode**, the simplest of the three
electrical modes the SD card specification defines. The other two
(1-bit SD bus and 4-bit SD bus) require a multi-line bidirectional
controller and a clock that the FPGC's existing SPI cores cannot
produce, so the SPI mode trade-off — slower but trivial to drive —
is the right fit.

| Pin       | SPI-mode role             |
|-----------|---------------------------|
| `CS`      | chip select (active low)  |
| `CLK`     | serial clock              |
| `DI`      | host → card (MOSI)        |
| `DO`      | card → host (MISO)        |
| `VDD/VSS` | 3.3 V / GND               |

The card-detect switch in the slot is not routed in the current PCB
revision, so software cannot tell whether a card is inserted other
than by attempting to initialise it.

The SPI clock runs at the same divider as the BRFS flash on
`SPI1` — fast enough to be useful but well within the SD spec for
SPI mode. The SD specification mandates that the **initial** clock
phase (before the card has been put into SPI mode) be no faster
than 400 kHz; in practice every card the project has been tested
against tolerates the higher clock from cycle one, and the FPGC does
not implement a slow-init phase.

## Logical layout

Every SD card presents itself as a flat array of **512-byte logical
blocks**. Block 0 is the master boot record / boot sector if the
card has been formatted with a partition table; the FPGC does not
currently parse partition tables and treats the card as raw block
storage.

Capacities below 2 GiB use byte addressing (SDSC). Capacities of
2 GiB and above use block addressing (SDHC up to 32 GiB, SDXC above
that). The FPGC's driver hard-rejects byte-addressed cards during
init, so every supported card sees the same address space: pass an
LBA in, get a 512-byte block back.

The card has its own internal flash translation layer, wear
levelling, and erase scheduling. The host never sees the underlying
NAND geometry and never has to issue an erase before overwriting a
block — an in-place rewrite is always sufficient.

## Initialisation

Bringing a card from cold power-up to ready takes a small choreographed
sequence. The card boots in 1-bit SD-bus mode and only switches to
SPI mode once it has seen `CMD0` (`GO_IDLE_STATE`) with `CS` held
low. Before that, `CS` must stay deasserted while at least 74 clock
pulses are sent — the spec calls these the "initial dummy clocks"
and they give the card's internal regulator time to reach a stable
state.

The full sequence is:

1. Hold `CS` high, send 80 dummy clocks (10 bytes of `0xFF`).
2. Pull `CS` low, send `CMD0` (`GO_IDLE_STATE`) with the canonical
   precomputed CRC. The card responds with `R1 = 0x01` to confirm
   it has entered SPI mode.
3. Send `CMD8` (`SEND_IF_COND`) with the voltage and check-pattern
   arguments specified by the SD spec. Cards that don't respond, or
   that flag CMD8 as illegal, are pre-spec-2.0 (SDSC) and are
   rejected at this point.
4. Loop on `ACMD41` (`SEND_OP_COND`) with the High Capacity Support
   bit set, until the card returns `R1 = 0x00` (ready) or the
   retry budget runs out.
5. Issue `CMD58` (`READ_OCR`) and inspect the **CCS** bit in the
   returned 32-bit OCR. CCS = 0 means SDSC byte-addressing — also
   rejected. CCS = 1 means SDHC/SDXC block-addressing, which is
   what we want.

Once init is complete the driver pulls capacity (in 512-byte
blocks) out of the **CSD** register via `CMD9`, leaves the card
selected only for the duration of each later command, and is ready
to serve block reads and writes.

## Command set

A small subset of the full SD command set is enough for a block
device:

| CMD    | Name                  | Direction        | Purpose                                |
|--------|-----------------------|------------------|----------------------------------------|
| `CMD0` | `GO_IDLE_STATE`       | host → card      | Reset; required first-ever command     |
| `CMD8` | `SEND_IF_COND`        | host → card      | Probe for ≥ spec 2.0 compliance        |
| `CMD9` | `SEND_CSD`            | card → host      | Read card-specific data (capacity)     |
| `CMD12`| `STOP_TRANSMISSION`   | host → card      | Stop a multi-block stream              |
| `CMD16`| `SET_BLOCKLEN`        | host → card      | Defensive; ignored on SDHC/SDXC        |
| `CMD17`| `READ_SINGLE_BLOCK`   | card → host      | Read one 512-byte block                |
| `CMD18`| `READ_MULTIPLE_BLOCK` | card → host      | Stream consecutive 512-byte blocks     |
| `CMD24`| `WRITE_BLOCK`         | host → card      | Write one 512-byte block               |
| `CMD25`| `WRITE_MULTIPLE_BLOCK`| host → card      | Stream consecutive blocks              |
| `CMD55`| `APP_CMD`             | host → card      | Marks the next command as ACMDxx       |
| `ACMD41`| `SD_SEND_OP_COND`    | host → card      | Init "ready?" loop                     |
| `CMD58`| `READ_OCR`            | card → host      | Read operating conditions register     |
| `CMD59`| `CRC_ON_OFF`          | host → card      | Toggle bus CRC checking                |

Every command frame is six bytes: `0x40 | cmd_index`, then a 32-bit
big-endian argument, then a CRC7 byte with the LSB always set to 1.
After a command the host clocks `0xFF` bytes until it sees an `R1`
response (top bit clear). Commands that return more than `R1`
(`CMD8`, `CMD58`) are followed by a fixed number of trailing data
bytes; commands that move a block of data are followed by a start
token (`0xFE` for reads, `0xFE`/`0xFC` for writes) and the 512-byte
payload plus a 16-bit CRC.

## Read / write protocol

A single-block **read** (`CMD17`) flows like this:

1. Host sends `CMD17` with the LBA as argument.
2. Card returns `R1 = 0x00`.
3. Card streams `0xFF` bytes (busy filler) until it has the data
   ready, then sends the **start token** `0xFE`.
4. Card streams 512 data bytes followed by a 16-bit CRC.
5. Host deselects `CS` and sends one more dummy byte to release
   the bus.

A single-block **write** (`CMD24`) is the mirror image: host sends
the command, gets `R1`, sends one gap byte and the start token,
then 512 payload bytes and a CRC. The card replies with a 1-byte
data response token (`0x05` = accepted, `0x0B` = CRC error,
`0x0D` = write error). While the card programs the block it holds
`DO` low; the host clocks `0xFF` until `DO` floats high again.

Multi-block transfers (`CMD18` / `CMD25`) follow the same per-block
shape but stay open until the host explicitly stops the stream
(with `CMD12` for reads, or with the `0xFD` "stop tran" token for
writes).

## DMA path

The `SPI5` controller is wired into the [DMA engine](../DMA.md)'s
SPI burst port, with the same `MEM2SPI` and `SPI2MEM` modes used by
the BRFS flash. Software issues the command, waits for the start
token, and then hands the 512-byte payload to the DMA. The CPU is
free during the transfer and only re-engages to drain the trailing
CRC bytes and to poll the busy line on writes.

This matters more for writes than for reads: a 512-byte write is
followed by a card-internal program operation that can take several
milliseconds, and being able to do unrelated work during the
program phase keeps the rest of the system responsive.

## Reliability and recovery

SD cards can recover from a power loss between the host's last
write and the card's internal commit, but not always cleanly. The
SD specification mandates an internal write journal but leaves
recovery semantics card-specific. Software that uses the SD card
for file storage should treat any in-flight write as
non-persistent until the next read confirms it.

The card's internal controller can also reorder erases and remaps;
the host has no insight into when this is happening other than
through occasional longer-than-normal busy periods on a write.

If the host issues a malformed command or ends a transfer at the
wrong place, the card can become wedged in a transitional state
where subsequent commands fail with a "protocol" response code.
Power-cycling the slot (in practice, power-cycling the whole
board) is the only reliable way to reset the card from this
state — the SD spec has no software-only "go back to idle" command
that is guaranteed to work after a multi-block stream has gone
sideways.

## Future use

The SD card driver and its block-oriented vtable wrapper are in
place but the BRFS filesystem currently only mounts the on-board
SPI flash. Wiring the card up as a second BRFS volume — with the
much larger capacity available on a microSD — is a future
filesystem-layer change; nothing in the hardware or driver layers
needs to move first.
