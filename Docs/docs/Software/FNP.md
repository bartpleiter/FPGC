# FNP (FPGC Network Protocol)

FNP is a custom Layer 2 Ethernet protocol for network communication with the FPGC, built on top of the ENC28J60 Ethernet controller. It is currently used for uploading files to the FPGC and sending keyboard input, but it is designed to be extensible for other applications like FPGC-to-FPGC messaging.

## Protocol Overview

FNP operates directly over Ethernet using raw frames with EtherType `0xB4B4`. It avoids IP/TCP/UDP entirely to keep complexity low and reduce overhead on the CPU.

### Frame Structure

!!! note
    Note that these sizes are in bytes, not words, as the ENC28J60 operates on byte-level data. Therefore, the maximum data payload of 1024 bytes corresponds to 256 words when interpreted by the FPGC.

Every FNP frame is a standard Ethernet frame with a 7-byte FNP header in the payload:

```
Ethernet: [Dest MAC (6)] [Src MAC (6)] [0xB4B4 (2)] [FNP Payload] [CRC (4)]

FNP Payload: [Version (1)] [Type (1)] [Seq (2)] [Flags (1)] [Length (2)] [Data (0-1024)]
```

- **Version**: Always `0x01`
- **Type**: Message type (see below)
- **Seq**: 16-bit sequence number for ACK matching (increments per message sent; ACK/NACK always use `0x0000`)
- **Flags**: `0x02` = REQUIRES_ACK, `0x01` = MORE_DATA (more chunks follow)
- **Length**: Data field size in bytes
- **Data**: Up to 1024 bytes (256 words), message type-specific

### MAC Address Convention

FPGC devices use MAC prefix `02:B4:B4:00:00:xx` where `xx` identifies the device based on a hardcoded mapping of a byte in the unique ID of the SPI Flash 0 chip. There are currently no plans for discovery as the FPGC devices are known, and the communication is expected to be between FPGC devices or from a PC to a single FPGC.

### Message Types

| Type | Name | Data Layout | Description |
|------|------|-------------|-------------|
| `0x01` | ACK | `[Acked Seq (2)]` | Acknowledge receipt |
| `0x02` | NACK | `[Nacked Seq (2)] [Error Code (1)] [Error String]` | Reject with error |
| `0x10` | FILE_START | `[Path Len (2)] [File Size in words (4)] [Path String]` | Begin file transfer |
| `0x11` | FILE_DATA | `[Word-packed data (4N bytes)]` | File data chunk (max 1024 bytes = 256 words) |
| `0x12` | FILE_END | `[Checksum (4)]` | End transfer; checksum = 32-bit sum of all words |
| `0x13` | FILE_ABORT | (empty) | Abort in-progress transfer |
| `0x20` | KEYCODE | `[Keycode (2)]` | HID keycode input |
| `0x30` | MESSAGE | Application-defined | FPGC-to-FPGC messaging |

### File Transfer Flow

1. Sender sends FILE_START with path and total size in words. Receiver ACKs.
2. Sender sends FILE_DATA chunks sequentially (max 256 words each), waiting for ACK after each.
3. Sender sends FILE_END with checksum. Receiver verifies and ACKs.

Binary files are packed into 32-bit words (big-endian: first byte in bits 31–24). Text files are converted on the host PC side to contain one ASCII character per word.

### Reliability

All messages with REQUIRES_ACK set follow: send -> wait 100ms for ACK/NACK -> retry up to 2 times (3 total attempts). ACK-pacing naturally limits throughput to what the FPGC can handle.

### Packet Reception

Incoming Ethernet packets are handled by an interrupt-driven architecture. The ENC28J60 triggers a hardware interrupt when a packet arrives, and the BDOS ISR immediately drains all pending packets from the ENC28J60's small hardware RX buffer (6656 bytes, roughly 6 max-size frames) into a 64-slot ring buffer in SDRAM. This prevents packet loss during CPU-intensive operations like screen rendering or computation.

Both the kernel FNP handler and user programs read from this ring buffer rather than accessing the ENC28J60 directly. See [OS](OS.md) for details on the interrupt handling and syscall interface.

## Setup

FNP uses raw Ethernet frames, and communication goes via Python scripts, so setting permissions to send raw packets without root is required for the makefile scripts to work properly:

```bash
sudo setcap cap_net_raw+ep $(readlink -f .venv/bin/python3)
```
