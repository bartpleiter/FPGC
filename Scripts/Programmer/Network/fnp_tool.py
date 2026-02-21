#!/usr/bin/env python3
"""
fnp_tool.py — FPGC Network Protocol (FNP) PC-side tool.

Supports file upload and remote keyboard input to an FPGC device over
raw Ethernet frames using the FNP protocol (EtherType 0xB4B4).

Usage:
  python fnp_tool.py <interface> upload [-t] <local_file> <fpgc_path>
  python fnp_tool.py <interface> key <text>
  python fnp_tool.py <interface> keycode <hex_code>

The -t / --text flag on upload treats the file as text: each byte is
stored as a full 32-bit word (one character per word) matching how the
FPGC's word-addressable memory represents strings.

Requires root unless you do something like `sudo setcap cap_net_raw+ep /usr/bin/python3.13`
"""

import fcntl
import math
import os
import socket
import struct
import sys
import time

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


def print_usage():
    print(__doc__)
    print("Options:")
    print("  --mac XX:XX:XX:XX:XX:XX   FPGC MAC address (default: 02:B4:B4:00:00:01)")
    print("  -t, --text                Upload as text (one char per word)")
    print()
    print("Examples:")
    print("  python fnp_tool.py eth0 upload firmware.bin /user/firmware.bin")
    print("  python fnp_tool.py eth0 upload -t readme.txt /user/readme.txt")
    print("  python fnp_tool.py eth0 key 'hello world'")
    print("  python fnp_tool.py eth0 keycode 0x0041")


def parse_mac(mac_str: str) -> bytes:
    """Parse a MAC address string like '02:B4:B4:00:00:01' into bytes."""
    parts = mac_str.split(":")
    if len(parts) != 6:
        raise ValueError(f"Invalid MAC address: {mac_str}")
    return bytes(int(p, 16) for p in parts)


def main():
    if len(sys.argv) < 3:
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

    if len(args) < 2:
        print_usage()
        sys.exit(1)

    iface = args[0]
    cmd = args[1]

    conn = FNPConnection(iface, fpgc_mac)

    try:
        if cmd == "upload":
            # Parse -t/--text flag
            upload_args = args[2:]
            text_mode = False
            if "-t" in upload_args:
                text_mode = True
                upload_args.remove("-t")
            if "--text" in upload_args:
                text_mode = True
                upload_args.remove("--text")

            if len(upload_args) < 2:
                print("Usage: fnp_tool.py <iface> upload [-t] <local_file> <fpgc_path>")
                sys.exit(1)
            local_file = upload_args[0]
            fpgc_path = upload_args[1]

            if not os.path.isfile(local_file):
                print(f"Error: file not found: {local_file}", file=sys.stderr)
                sys.exit(1)

            success = conn.upload_file(local_file, fpgc_path, text_mode=text_mode)
            sys.exit(0 if success else 1)

        elif cmd == "key":
            if len(args) < 3:
                print("Usage: fnp_tool.py <iface> key <text>")
                sys.exit(1)
            text = " ".join(args[2:])
            success = conn.send_text(text)
            sys.exit(0 if success else 1)

        elif cmd == "keycode":
            if len(args) < 3:
                print("Usage: fnp_tool.py <iface> keycode <hex_code>")
                sys.exit(1)
            code = int(args[2], 0)  # Supports 0x prefix
            success = conn.send_keycode(code)
            print(f"Sent keycode 0x{code:04X}: {'OK' if success else 'FAILED'}")
            sys.exit(0 if success else 1)

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
