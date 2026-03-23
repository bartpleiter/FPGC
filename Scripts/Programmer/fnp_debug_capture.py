#!/usr/bin/env python3
"""
FNP Debug Capture — compile, upload, run a program on FPGC via FNP and capture UART output.

Opens the UART port first (which resets the CH340-connected FPGC), waits for
the device to boot, then uploads the binary via FNP, runs it, and captures
UART debug output.

Usage:
    python3 fnp_debug_capture.py --mac 02:B4:B4:00:00:01 --cmd w3d --bin code.bin --dest /bin/w3d --duration 3
"""

import argparse
import subprocess
import sys
import time
from pathlib import Path

import serial

PROJECT_ROOT = Path(__file__).resolve().parent.parent.parent
FNP_TOOL = PROJECT_ROOT / "Scripts" / "Programmer" / "Network" / "fnp_tool.py"
PYTHON = PROJECT_ROOT / ".venv" / "bin" / "python3"

BOOT_WAIT = 11  # seconds to wait for the FPGC to boot after reset


def fnp_upload(mac: str, local_path: str, fpgc_path: str) -> None:
    """Upload a binary to the FPGC via FNP."""
    subprocess.run(
        [str(PYTHON), str(FNP_TOOL), "--mac", mac, "upload", local_path, fpgc_path],
        check=True,
    )


def fnp_send_key(mac: str, text: str) -> None:
    """Send a string as FNP keycodes."""
    subprocess.run(
        [str(PYTHON), str(FNP_TOOL), "--mac", mac, "key", text],
        check=True,
        capture_output=True,
    )


def fnp_send_keycode(mac: str, keycode: int) -> None:
    """Send a single FNP keycode."""
    subprocess.run(
        [str(PYTHON), str(FNP_TOOL), "--mac", mac, "keycode", hex(keycode)],
        check=True,
        capture_output=True,
    )


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Upload, run an FPGC program via FNP and capture UART debug output"
    )
    parser.add_argument("--mac", required=True, help="Target FPGC MAC address")
    parser.add_argument("--cmd", required=True, help="Program name to run on FPGC")
    parser.add_argument("--bin", required=True, help="Path to compiled binary")
    parser.add_argument("--dest", required=True, help="FPGC destination path (e.g. /bin/hello)")
    parser.add_argument(
        "--duration",
        type=float,
        default=5.0,
        help="Seconds to capture UART output (default: 5)",
    )
    parser.add_argument(
        "--port", default="/dev/ttyUSB0", help="UART serial port (default: /dev/ttyUSB0)"
    )
    parser.add_argument(
        "--baudrate", type=int, default=1000000, help="UART baudrate (default: 1000000)"
    )
    args = parser.parse_args()

    # Step 1: Open UART (this resets the FPGC on CH340 adapters via DTR)
    print(f"Opening UART {args.port} (device will reset)...", file=sys.stderr)
    try:
        ser = serial.Serial(args.port, args.baudrate, timeout=0.1)
    except serial.SerialException as e:
        print(f"UART error: {e}", file=sys.stderr)
        return 1

    try:
        # Step 2: Wait for device to boot
        print(f"Waiting {BOOT_WAIT}s for FPGC to boot...", file=sys.stderr)
        time.sleep(BOOT_WAIT)

        # Flush any boot output
        ser.reset_input_buffer()

        # Step 3: Upload binary via FNP
        print(f"Uploading {args.bin} to {args.dest}...", file=sys.stderr)
        fnp_upload(args.mac, args.bin, args.dest)

        # Step 4: Run the program via FNP keycodes
        print(f"Running '{args.cmd}'...", file=sys.stderr)
        fnp_send_key(args.mac, args.cmd)
        fnp_send_keycode(args.mac, 0x0A)

        # Step 5: Capture UART output
        print(f"Capturing UART output for {args.duration}s...", file=sys.stderr)
        output = []
        start = time.time()
        while (time.time() - start) < args.duration:
            data = ser.read(ser.in_waiting or 1)
            if data:
                output.append(data.decode(errors="replace"))

    except subprocess.CalledProcessError as e:
        print(f"FNP error: {e}", file=sys.stderr)
        return 1
    finally:
        ser.close()

    # Print captured output to stdout
    result = "".join(output)
    print(result, end="")
    return 0


if __name__ == "__main__":
    sys.exit(main())
