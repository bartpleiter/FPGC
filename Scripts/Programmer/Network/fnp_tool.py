#!/usr/bin/env python3
"""
fnp_tool.py — FPGC Network Protocol (FNP) PC-side tool.

Supports file upload, remote keyboard input, and interactive keyboard
streaming to an FPGC device over raw Ethernet frames using the FNP
protocol (EtherType 0xB4B4).

Usage:
  python fnp_tool.py [<interface>] upload [-t] <local_file> <fpgc_path>
  python fnp_tool.py [<interface>] key <text>
  python fnp_tool.py [<interface>] keycode <hex_code>
  python fnp_tool.py [<interface>] keyboard
  python fnp_tool.py [<interface>] abort
  python fnp_tool.py detect-iface

If <interface> is omitted, the tool auto-detects a USB Ethernet adapter.

The -t / --text flag on upload treats the file as text: each byte is
stored as a full 32-bit word (one character per word) matching how the
FPGC's word-addressable memory represents strings.

Requires raw socket capability. Either run as root, or grant it with:
  sudo setcap cap_net_raw+ep $(which python3)
"""

import fcntl
import math
import os
import select
import socket
import struct
import sys
import termios
import time
import tty

# ---- FNP Protocol Constants ----

ETHERTYPE_FNP = 0xB4B4
FNP_VERSION = 0x01
FNP_HEADER_SIZE = 7
ETH_HEADER_SIZE = 14

# Message types
FNP_TYPE_ACK = 0x01
FNP_TYPE_NACK = 0x02
FNP_TYPE_FILE_START = 0x10
FNP_TYPE_FILE_DATA = 0x11
FNP_TYPE_FILE_END = 0x12
FNP_TYPE_FILE_ABORT = 0x13
FNP_TYPE_KEYCODE = 0x20
FNP_TYPE_MESSAGE = 0x30

# Flags
FNP_FLAG_MORE_DATA = 0x01
FNP_FLAG_REQUIRES_ACK = 0x02

# Error codes
FNP_ERR_GENERIC = 0xFF

# Timing
ACK_TIMEOUT = 0.1  # 100ms
MAX_RETRIES = 2  # 3 total attempts

# Chunk size for FILE_DATA (1024 bytes = 256 words)
FILE_CHUNK_SIZE = 1024

# Default FPGC MAC
FPGC_MAC = bytes([0x02, 0xB4, 0xB4, 0x00, 0x00, 0x01])

# HID keycode mapping for keyboard streaming mode
# Maps terminal escape sequences and characters to FPGC HID-style keycodes.
# Standard printable ASCII maps directly (0x20–0x7E).
# Special keys use well-known HID codes (shifted to upper byte if needed).
HID_KEY_ENTER = 0x0A
HID_KEY_BACKSPACE = 0x08
HID_KEY_TAB = 0x09
HID_KEY_ESCAPE = 0x1B
HID_KEY_UP = 0xF700
HID_KEY_DOWN = 0xF701
HID_KEY_RIGHT = 0xF702
HID_KEY_LEFT = 0xF703
HID_KEY_DELETE = 0x7F

# Map escape sequences to keycodes
ESCAPE_SEQ_MAP: dict[bytes, int] = {
    b"\x1b[A": HID_KEY_UP,
    b"\x1b[B": HID_KEY_DOWN,
    b"\x1b[C": HID_KEY_RIGHT,
    b"\x1b[D": HID_KEY_LEFT,
    b"\x1b[3~": HID_KEY_DELETE,
}


def detect_interface() -> str:
    """
    Auto-detect a USB Ethernet adapter interface.

    Looks for network interfaces that appear to be USB Ethernet adapters
    (names starting with 'enx' on systemd-based Linux, which encodes the
    MAC address in the interface name). Falls back to any non-loopback,
    non-wireless Ethernet interface if no 'enx' interface is found.
    """
    import subprocess

    result = subprocess.run(
        ["ip", "-o", "link", "show"],
        capture_output=True,
        text=True,
        check=True,
    )

    interfaces: list[str] = []
    fallback: list[str] = []

    for line in result.stdout.strip().split("\n"):
        # Format: "2: eth0: <FLAGS> ..."
        parts = line.split(": ")
        if len(parts) < 2:
            continue
        name = parts[1].split("@")[0]  # Handle e.g. "enx...@if2"

        if name == "lo":
            continue

        # Check if interface is UP
        if "UP" not in line:
            continue

        # Primary: USB Ethernet adapters (systemd naming)
        if name.startswith("enx"):
            interfaces.append(name)
        # Fallback: wired Ethernet (not wireless)
        elif name.startswith(("eth", "en")) and not name.startswith("enp0s"):
            fallback.append(name)

    if interfaces:
        if len(interfaces) > 1:
            print(
                f"Warning: multiple USB Ethernet adapters found: {interfaces}",
                file=sys.stderr,
            )
            print(f"Using: {interfaces[0]}", file=sys.stderr)
        return interfaces[0]

    if fallback:
        return fallback[0]

    print(
        "Error: could not auto-detect Ethernet interface.\n"
        "Available interfaces:",
        file=sys.stderr,
    )
    result2 = subprocess.run(
        ["ip", "-o", "link", "show"],
        capture_output=True,
        text=True,
        check=True,
    )
    for line in result2.stdout.strip().split("\n"):
        parts = line.split(": ")
        if len(parts) >= 2:
            print(f"  {parts[1].split('@')[0]}", file=sys.stderr)
    sys.exit(1)


def get_mac(sock: socket.socket, iface: str) -> bytes:
    """Get the MAC address of the given network interface."""
    info = fcntl.ioctl(sock.fileno(), 0x8927, struct.pack("256s", iface.encode()[:15]))
    return info[18:24]


class FNPConnection:
    """Manages an FNP connection to a single FPGC device."""

    def __init__(self, iface: str, fpgc_mac: bytes = FPGC_MAC):
        self.iface = iface
        self.fpgc_mac = fpgc_mac
        self.seq = 0

        self.sock = socket.socket(
            socket.AF_PACKET, socket.SOCK_RAW, socket.htons(ETHERTYPE_FNP)
        )
        self.sock.bind((iface, 0))
        self.src_mac = get_mac(self.sock, iface)

    def close(self):
        self.sock.close()

    def _next_seq(self) -> int:
        """Get next sequence number and increment counter."""
        seq = self.seq
        self.seq = (self.seq + 1) & 0xFFFF
        return seq

    def _build_frame(
        self,
        msg_type: int,
        seq: int,
        flags: int,
        data: bytes,
    ) -> bytes:
        """Build a complete Ethernet + FNP frame."""
        # Ethernet header
        eth_header = self.fpgc_mac + self.src_mac + struct.pack("!H", ETHERTYPE_FNP)

        # FNP header: version(1) + type(1) + seq(2) + flags(1) + length(2)
        fnp_header = struct.pack("!BBHBH", FNP_VERSION, msg_type, seq, flags, len(data))

        return eth_header + fnp_header + data

    def _send_raw(self, frame: bytes):
        """Send a raw Ethernet frame."""
        self.sock.send(frame)

    def _recv_frame(self, timeout: float) -> tuple | None:
        """
        Receive and parse an FNP frame.
        Returns (msg_type, seq, flags, data) or None on timeout.
        """
        self.sock.settimeout(timeout)
        try:
            raw, _addr = self.sock.recvfrom(2048)
        except socket.timeout:
            return None

        if len(raw) < ETH_HEADER_SIZE + FNP_HEADER_SIZE:
            return None

        ethertype = struct.unpack("!H", raw[12:14])[0]
        if ethertype != ETHERTYPE_FNP:
            return None

        version, msg_type, seq, flags, data_len = struct.unpack(
            "!BBHBH", raw[ETH_HEADER_SIZE : ETH_HEADER_SIZE + FNP_HEADER_SIZE]
        )

        if version != FNP_VERSION:
            return None

        data = raw[ETH_HEADER_SIZE + FNP_HEADER_SIZE :]
        if len(data) < data_len:
            return None

        return (msg_type, seq, flags, data[:data_len])

    def _send_and_wait_ack(
        self,
        msg_type: int,
        flags: int,
        data: bytes,
    ) -> bool:
        """
        Send a message with REQUIRES_ACK and wait for ACK.
        Retries up to MAX_RETRIES times.
        Returns True on ACK, False on failure/NACK.
        """
        seq = self._next_seq()
        frame = self._build_frame(msg_type, seq, flags | FNP_FLAG_REQUIRES_ACK, data)

        for attempt in range(MAX_RETRIES + 1):
            self._send_raw(frame)

            # Wait for matching ACK/NACK
            deadline = time.time() + ACK_TIMEOUT
            while time.time() < deadline:
                remaining = deadline - time.time()
                if remaining <= 0:
                    break

                result = self._recv_frame(remaining)
                if result is None:
                    continue

                r_type, r_seq, r_flags, r_data = result

                if r_type == FNP_TYPE_ACK and len(r_data) >= 2:
                    acked_seq = struct.unpack("!H", r_data[:2])[0]
                    if acked_seq == seq:
                        return True

                if r_type == FNP_TYPE_NACK and len(r_data) >= 3:
                    nacked_seq = struct.unpack("!H", r_data[:2])[0]
                    if nacked_seq == seq:
                        error_code = r_data[2]
                        error_msg = ""
                        if len(r_data) > 3:
                            error_msg = (
                                r_data[3:]
                                .split(b"\x00")[0]
                                .decode("ascii", errors="replace")
                            )
                        print(
                            f"  NACK received: error=0x{error_code:02X} {error_msg}",
                            file=sys.stderr,
                        )
                        return False

            if attempt < MAX_RETRIES:
                print(
                    f"  Timeout, retry {attempt + 1}/{MAX_RETRIES}...",
                    file=sys.stderr,
                )

        print("  Failed: no ACK after all retries", file=sys.stderr)
        return False

    # ---- Public API ----

    def upload_file(
        self, local_path: str, fpgc_path: str, text_mode: bool = False
    ) -> bool:
        """
        Upload a file to the FPGC using the FNP file transfer protocol.

        In binary mode (default): file bytes are packed 4-per-word (big-endian).
        In text mode (-t): each byte becomes one 32-bit word (one char per word),
        matching B32P3's word-addressable char representation.
        """
        # Read file
        with open(local_path, "rb") as f:
            file_bytes = f.read()

        file_size_bytes = len(file_bytes)

        if text_mode:
            # Text mode: each byte becomes a 32-bit big-endian word
            # e.g. byte 0x48 ('H') -> word 0x00000048
            word_count = len(file_bytes)
            packed = b""
            for b in file_bytes:
                packed += struct.pack("!I", b)
            file_bytes = packed
            mode_str = "text"
        else:
            # Binary mode: pack 4 bytes per word, pad to multiple of 4
            pad_len = (4 - (file_size_bytes % 4)) % 4
            file_bytes += b"\x00" * pad_len
            word_count = len(file_bytes) // 4
            mode_str = "binary"

        print(
            f"Uploading {local_path} ({file_size_bytes} bytes, {word_count} words, "
            f"{mode_str} mode) -> {fpgc_path}"
        )

        # Compute checksum over packed words
        checksum = 0
        for i in range(word_count):
            word = struct.unpack("!I", file_bytes[i * 4 : i * 4 + 4])[0]
            checksum = (checksum + word) & 0xFFFFFFFF

        # FILE_START: path_len(2) + file_size_words(4) + path(N)
        path_bytes = fpgc_path.encode("ascii") + b"\x00"
        start_data = struct.pack("!HI", len(path_bytes), word_count) + path_bytes

        print("  Sending FILE_START...")
        if not self._send_and_wait_ack(FNP_TYPE_FILE_START, 0, start_data):
            print("  FILE_START failed", file=sys.stderr)
            return False

        # FILE_DATA chunks
        total_chunks = math.ceil(len(file_bytes) / FILE_CHUNK_SIZE)
        for chunk_idx in range(total_chunks):
            offset = chunk_idx * FILE_CHUNK_SIZE
            chunk = file_bytes[offset : offset + FILE_CHUNK_SIZE]

            is_last = chunk_idx == total_chunks - 1
            flags = 0 if is_last else FNP_FLAG_MORE_DATA

            pct = int((chunk_idx + 1) / total_chunks * 100)
            print(
                f"  Sending chunk {chunk_idx + 1}/{total_chunks} "
                f"({len(chunk)} bytes) [{pct}%]"
            )

            if not self._send_and_wait_ack(FNP_TYPE_FILE_DATA, flags, chunk):
                print(f"  FILE_DATA chunk {chunk_idx + 1} failed", file=sys.stderr)
                # Try to abort
                self._send_and_wait_ack(FNP_TYPE_FILE_ABORT, 0, b"")
                return False

        # FILE_END: checksum(4)
        end_data = struct.pack("!I", checksum)
        print(f"  Sending FILE_END (checksum=0x{checksum:08X})...")
        if not self._send_and_wait_ack(FNP_TYPE_FILE_END, 0, end_data):
            print("  FILE_END failed", file=sys.stderr)
            return False

        print("  Upload complete!")
        return True

    def send_keycode(self, keycode: int) -> bool:
        """Send a single HID keycode to the FPGC."""
        data = struct.pack("!H", keycode)
        return self._send_and_wait_ack(FNP_TYPE_KEYCODE, 0, data)

    def send_text(self, text: str) -> bool:
        """Send a string as individual keycode events (ASCII values)."""
        for ch in text:
            code = ord(ch)
            print(f"  Sending keycode 0x{code:04X} ('{ch}')")
            if not self.send_keycode(code):
                print(f"  Failed to send '{ch}'", file=sys.stderr)
                return False
        return True

    def abort_transfer(self) -> bool:
        """Send FILE_ABORT to cancel any in-progress transfer."""
        return self._send_and_wait_ack(FNP_TYPE_FILE_ABORT, 0, b"")

    def keyboard_stream(self) -> None:
        """
        Interactive keyboard streaming mode.

        Puts the terminal in raw mode and forwards every keypress to the
        FPGC as KEYCODE messages in real-time.  Press Ctrl+C to exit.
        """
        print("Keyboard streaming mode — press Ctrl+C to exit.")
        print(f"  Interface: {self.iface}")
        print(f"  FPGC MAC:  {':'.join(f'{b:02X}' for b in self.fpgc_mac)}")
        print()

        old_settings = termios.tcgetattr(sys.stdin)
        try:
            tty.setraw(sys.stdin)
            buf = b""

            while True:
                # Wait for input (stdin fd=0)
                rlist, _, _ = select.select([sys.stdin], [], [], 0.05)
                if not rlist:
                    # Process any buffered escape sequence on timeout
                    if buf:
                        self._process_key_buffer(buf)
                        buf = b""
                    continue

                ch = os.read(sys.stdin.fileno(), 1)
                if not ch:
                    continue

                # Ctrl+C → exit
                if ch == b"\x03":
                    break

                buf += ch

                # Check if buffer matches a known escape sequence
                if buf[0:1] == b"\x1b":
                    # Check for complete match
                    matched = False
                    for seq, _code in ESCAPE_SEQ_MAP.items():
                        if buf == seq:
                            self._process_key_buffer(buf)
                            buf = b""
                            matched = True
                            break
                    if matched:
                        continue

                    # Check if buffer could still become a match
                    prefix_match = any(
                        seq.startswith(buf) for seq in ESCAPE_SEQ_MAP
                    )
                    if prefix_match:
                        continue  # Wait for more bytes

                    # No match possible — send what we have
                    self._process_key_buffer(buf)
                    buf = b""
                else:
                    # Regular character — send immediately
                    self._process_key_buffer(buf)
                    buf = b""

        except KeyboardInterrupt:
            pass
        finally:
            termios.tcsetattr(sys.stdin, termios.TCSADRAIN, old_settings)
            print("\nKeyboard streaming ended.")

    def _process_key_buffer(self, buf: bytes) -> None:
        """Process a key buffer and send the appropriate keycode(s)."""
        # Check for escape sequence match first
        if buf in ESCAPE_SEQ_MAP:
            code = ESCAPE_SEQ_MAP[buf]
            self.send_keycode(code)
            return

        # Process individual characters
        for b in buf:
            if b == 0x0D:  # Enter (CR in raw mode)
                code = HID_KEY_ENTER
            elif b == 0x7F:  # Backspace (DEL in raw mode)
                code = HID_KEY_BACKSPACE
            else:
                code = b  # Direct ASCII
            self.send_keycode(code)


def print_usage():
    print(__doc__)
    print("Commands:")
    print("  upload [-t] <local_file> <fpgc_path>   Upload a file")
    print("  key <text>                              Send text as keycodes")
    print("  keycode <hex_code>                      Send a single HID keycode")
    print("  keyboard                                Interactive keyboard streaming")
    print("  abort                                   Abort in-progress transfer")
    print("  detect-iface                            Print detected interface and exit")
    print()
    print("Options:")
    print("  --mac XX:XX:XX:XX:XX:XX   FPGC MAC address (default: 02:B4:B4:00:00:01)")
    print("  -t, --text                Upload as text (one char per word)")
    print()
    print("If <interface> is omitted, auto-detects a USB Ethernet adapter.")
    print()
    print("Examples:")
    print("  python fnp_tool.py upload firmware.bin /user/firmware.bin")
    print("  python fnp_tool.py upload -t readme.txt /user/readme.txt")
    print("  python fnp_tool.py keyboard")
    print("  python fnp_tool.py eth0 key 'hello world'")
    print("  python fnp_tool.py eth0 keycode 0x0041")


def parse_mac(mac_str: str) -> bytes:
    """Parse a MAC address string like '02:B4:B4:00:00:01' into bytes."""
    parts = mac_str.split(":")
    if len(parts) != 6:
        raise ValueError(f"Invalid MAC address: {mac_str}")
    return bytes(int(p, 16) for p in parts)


# Known FNP commands (used for auto-detect logic)
FNP_COMMANDS = {"upload", "key", "keycode", "keyboard", "abort", "detect-iface"}


def main():
    if len(sys.argv) < 2:
        print_usage()
        sys.exit(1)

    # Parse optional --mac flag
    args = list(sys.argv[1:])
    fpgc_mac = FPGC_MAC
    if "--mac" in args:
        idx = args.index("--mac")
        if idx + 1 >= len(args):
            print("Error: --mac requires a MAC address argument", file=sys.stderr)
            sys.exit(1)
        fpgc_mac = parse_mac(args[idx + 1])
        args = args[:idx] + args[idx + 2 :]

    if len(args) < 1:
        print_usage()
        sys.exit(1)

    # Determine if first arg is an interface name or a command.
    # If it's a known command, auto-detect the interface.
    if args[0] in FNP_COMMANDS:
        cmd = args[0]
        cmd_args = args[1:]

        # Special case: detect-iface doesn't need a connection
        if cmd == "detect-iface":
            iface = detect_interface()
            print(iface)
            sys.exit(0)

        iface = detect_interface()
        print(f"Auto-detected interface: {iface}")
    else:
        iface = args[0]
        if len(args) < 2:
            print_usage()
            sys.exit(1)
        cmd = args[1]
        cmd_args = args[2:]

    conn = FNPConnection(iface, fpgc_mac)

    try:
        if cmd == "upload":
            # Parse -t/--text flag
            upload_args = list(cmd_args)
            text_mode = False
            if "-t" in upload_args:
                text_mode = True
                upload_args.remove("-t")
            if "--text" in upload_args:
                text_mode = True
                upload_args.remove("--text")

            if len(upload_args) < 2:
                print(
                    "Usage: fnp_tool.py [<iface>] upload [-t] <local_file> <fpgc_path>"
                )
                sys.exit(1)
            local_file = upload_args[0]
            fpgc_path = upload_args[1]

            if not os.path.isfile(local_file):
                print(f"Error: file not found: {local_file}", file=sys.stderr)
                sys.exit(1)

            success = conn.upload_file(local_file, fpgc_path, text_mode=text_mode)
            sys.exit(0 if success else 1)

        elif cmd == "key":
            if len(cmd_args) < 1:
                print("Usage: fnp_tool.py [<iface>] key <text>")
                sys.exit(1)
            text = " ".join(cmd_args)
            success = conn.send_text(text)
            sys.exit(0 if success else 1)

        elif cmd == "keycode":
            if len(cmd_args) < 1:
                print("Usage: fnp_tool.py [<iface>] keycode <hex_code>")
                sys.exit(1)
            code = int(cmd_args[0], 0)  # Supports 0x prefix
            success = conn.send_keycode(code)
            print(f"Sent keycode 0x{code:04X}: {'OK' if success else 'FAILED'}")
            sys.exit(0 if success else 1)

        elif cmd == "keyboard":
            conn.keyboard_stream()
            sys.exit(0)

        elif cmd == "abort":
            success = conn.abort_transfer()
            print(f"Abort: {'OK' if success else 'FAILED'}")
            sys.exit(0 if success else 1)

        else:
            print(f"Unknown command: {cmd}")
            print_usage()
            sys.exit(1)

    finally:
        conn.close()


if __name__ == "__main__":
    main()
