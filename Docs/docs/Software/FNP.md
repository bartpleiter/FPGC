# FNP (FPGC Network Protocol)

FNP is a custom Layer 2 Ethernet protocol for communicating with the FPGC from a PC. It supports file uploads to BRFS, remote keyboard input, and program deployment — all over a direct Ethernet cable.

## Protocol Overview

FNP operates directly over Ethernet using raw frames with EtherType `0xB4B4`. It avoids IP/TCP/UDP entirely.

### Frame Structure

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
- **Data**: Up to 1024 bytes (256 words), type-specific

### MAC Address Convention

FPGC devices use MAC prefix `02:B4:B4:00:00:xx` where `xx` identifies the device. The PC uses its standard NIC MAC. No discovery — addresses are known.

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

Binary files are packed into 32-bit words (big-endian: first byte in bits 31–24). Text files are converted character-by-character (one ASCII character per word).

### Reliability

All messages with REQUIRES_ACK set follow: send → wait 100ms for ACK/NACK → retry up to 2 times (3 total attempts). ACK-pacing naturally limits throughput to what the FPGC can handle.

## Setup

FNP uses raw Ethernet frames, so the Python tool needs permission to send them:

```bash
sudo setcap cap_net_raw+ep $(readlink -f .venv/bin/python3)
```

Connect the FPGC to the PC via a USB Ethernet adapter. The tool auto-detects interfaces named `enx*`.

## fnp_tool.py

The main tool is at `Scripts/Programmer/Network/fnp_tool.py`.

### Commands

| Command | Description |
|---------|-------------|
| `upload <file> <dest>` | Upload a binary file to BRFS path `<dest>` |
| `upload -t <file> <dest>` | Upload a text file (ASCII→word conversion) |
| `key <char>` | Send a single keypress (e.g. `key a`) |
| `keycode <code>` | Send a raw HID keycode (decimal) |
| `keyboard` | Stream keyboard input to the FPGC in real-time |

### Examples

```bash
# Upload a binary to /bin/test.bin
python3 fnp_tool.py upload output.bin /bin/test.bin

# Upload a text file
python3 fnp_tool.py upload -t hello.txt /docs/hello.txt

# Stream keyboard (Ctrl+C to quit)
python3 fnp_tool.py keyboard

# Specify interface explicitly
python3 fnp_tool.py -i enx00e04c680001 upload file.bin /dest.bin
```

The interface is auto-detected by default. Use `-i <iface>` to override.

## Makefile Targets

| Target | Description |
|--------|-------------|
| `make fnp-upload-text file=<path> dest=<brfs_path>` | Upload a text file |
| `make fnp-upload-userbdos file=<name>` | Compile, assemble, and upload a userBDOS C program |
| `make fnp-keyboard` | Start keyboard streaming |
| `make fnp-detect-iface` | Print the detected Ethernet interface |

The `fnp-upload-userbdos` target automates the full pipeline: B32CC (`-user-bdos`) → ASMPY (`-h -i`) → fnp_tool.py upload to `/bin/<name>.bin`.

## Helper Scripts

Located in `Scripts/Programmer/Network/`:

- **fnp_upload_text.sh** — upload a text file
- **fnp_upload_userbdos.sh** — compile + assemble + upload a userBDOS program
- **fnp_keyboard.sh** — start keyboard streaming
