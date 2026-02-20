#!/usr/bin/env python3
"""
ethtest.py, simple test tool to validate FPGC ENC28J60 Ethernet driver

Sends and receives raw Ethernet frames using the custom FNP EtherType (0xB4B4)
to/from the FPGC board.

Usage:
  sudo python3 ethtest.py <interface> send      # Send a test frame to FPGC
  sudo python3 ethtest.py <interface> recv      # Listen for frames from FPGC
  sudo python3 ethtest.py <interface> loop      # Send, then listen for reply

Requires root (raw sockets).
"""

import socket
import struct
import sys

# FNP EtherType
ETHERTYPE_FNP = 0xB4B4

# FPGC MAC address
FPGC_MAC = bytes([0x02, 0xB4, 0xB4, 0x00, 0x00, 0x01])

# Broadcast MAC
BROADCAST_MAC = bytes([0xFF] * 6)


def get_mac(sock, iface):
    """Get the MAC address of the given interface."""
    import fcntl

    info = fcntl.ioctl(sock.fileno(), 0x8927, struct.pack("256s", iface.encode()[:15]))
    return info[18:24]


def send_test_frame(iface, broadcast=False):
    """Send a test frame with EtherType 0xB4B4 to the FPGC."""
    sock = socket.socket(socket.AF_PACKET, socket.SOCK_RAW, socket.htons(ETHERTYPE_FNP))
    sock.bind((iface, 0))

    src_mac = get_mac(sock, iface)
    dst_mac = BROADCAST_MAC if broadcast else FPGC_MAC

    # Build frame: dst + src + ethertype + payload
    payload = b"HELLO FROM PC"
    frame = dst_mac + src_mac + struct.pack("!H", ETHERTYPE_FNP) + payload

    sock.send(frame)
    print(f"Sent {len(frame)} bytes to {':'.join(f'{b:02X}' for b in dst_mac)}")
    print(f"  Src MAC: {':'.join(f'{b:02X}' for b in src_mac)}")
    print(f"  Payload: {payload}")
    sock.close()


def recv_frames(iface, timeout=10.0):
    """Listen for frames with EtherType 0xB4B4 from FPGC."""
    sock = socket.socket(socket.AF_PACKET, socket.SOCK_RAW, socket.htons(ETHERTYPE_FNP))
    sock.bind((iface, 0))
    sock.settimeout(timeout)

    print(
        f"Listening on {iface} for FNP frames (EtherType 0x{ETHERTYPE_FNP:04X}), timeout {timeout}s..."
    )

    try:
        while True:
            data, addr = sock.recvfrom(2048)
            dst_mac = data[0:6]
            src_mac = data[6:12]
            ethertype = struct.unpack("!H", data[12:14])[0]
            payload = data[14:]

            print(f"\nReceived {len(data)} bytes:")
            print(f"  Dst: {':'.join(f'{b:02X}' for b in dst_mac)}")
            print(f"  Src: {':'.join(f'{b:02X}' for b in src_mac)}")
            print(f"  EtherType: 0x{ethertype:04X}")
            print(f"  Payload ({len(payload)} bytes): {payload[:64]}")
            print(f"  Hex: {' '.join(f'{b:02X}' for b in payload[:64])}")
    except socket.timeout:
        print("Timeout - no more frames received.")
    finally:
        sock.close()


def loop_test(iface):
    """Send a frame, then listen for a reply."""
    send_test_frame(iface)
    print()
    recv_frames(iface, timeout=5.0)


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        print("Available interfaces:")
        import os

        for iface in os.listdir("/sys/class/net"):
            print(f"  {iface}")
        sys.exit(1)

    iface = sys.argv[1]
    cmd = sys.argv[2]

    if cmd == "send":
        broadcast = "--broadcast" in sys.argv or "-b" in sys.argv
        send_test_frame(iface, broadcast=broadcast)
    elif cmd == "recv":
        timeout = float(sys.argv[3]) if len(sys.argv) > 3 else 30.0
        recv_frames(iface, timeout)
    elif cmd == "loop":
        loop_test(iface)
    else:
        print(f"Unknown command: {cmd}")
        print("Use: send, recv, or loop")
        print("  send [-b|--broadcast]  Send test frame (use -b for broadcast dest)")
        sys.exit(1)


if __name__ == "__main__":
    main()
