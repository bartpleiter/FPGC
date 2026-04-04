#!/usr/bin/env python3
"""
UART Serial Monitor for FPGC
Reads and displays serial output from the FPGC UART port.
Useful for viewing debug print output from userBDOS programs.
"""

import argparse
import sys
import time

import serial
from serial.serialutil import SerialException


def main() -> int:
    parser = argparse.ArgumentParser(
        description="UART serial monitor for FPGC",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument(
        "-p", "--port", type=str, default="/dev/ttyUSB0", help="Serial port path"
    )
    parser.add_argument(
        "-b", "--baudrate", type=int, default=1000000, help="Serial baudrate"
    )
    parser.add_argument(
        "--hex", action="store_true", help="Also show hex values for each byte"
    )
    parser.add_argument(
        "--timestamp", action="store_true", help="Prefix each line with a timestamp"
    )
    args = parser.parse_args()

    try:
        if args.port.startswith("rfc2217://"):
            ser = serial.serial_for_url(
                args.port, baudrate=args.baudrate, timeout=0.1
            )
        else:
            ser = serial.Serial(args.port, baudrate=args.baudrate, timeout=0.1)
    except SerialException as e:
        print(f"Error: Failed to open {args.port}: {e}", file=sys.stderr)
        return 1

    print(
        f"--- UART Monitor on {args.port} @ {args.baudrate} baud. Press Ctrl+C to exit. ---",
        flush=True,
    )

    at_line_start = True
    try:
        while True:
            data = ser.read(256)
            if not data:
                continue

            if args.hex:
                for byte in data:
                    ch = chr(byte) if 0x20 <= byte < 0x7F or byte == 0x0A else "."
                    print(f"[{byte:02X}]{ch}", end="", flush=True)
            else:
                text = data.decode(errors="replace")
                if args.timestamp:
                    # Insert timestamp at beginning of each line
                    ts = time.strftime("%H:%M:%S")
                    result = []
                    for ch in text:
                        if at_line_start and ch != "\n":
                            result.append(f"[{ts}] ")
                        result.append(ch)
                        at_line_start = ch == "\n"
                    print("".join(result), end="", flush=True)
                else:
                    print(text, end="", flush=True)

    except KeyboardInterrupt:
        print("\n--- Monitor stopped. ---")
    except SerialException as e:
        print(f"\nError: Serial communication error: {e}", file=sys.stderr)
        return 1
    finally:
        ser.close()

    return 0


if __name__ == "__main__":
    sys.exit(main())
