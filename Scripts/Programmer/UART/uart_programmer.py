#!/usr/bin/env python3
"""
UART Programmer Tool for FPGC
A tool to program binary files onto FPGC via the UART bootloader.
"""

import argparse
import logging
import sys
import time
from pathlib import Path
from time import sleep
from typing import Optional

import serial
from serial.serialutil import SerialException


class UARTProgrammerError(Exception):
    """Custom exception for UART programmer errors."""

    pass


class UARTProgrammer:
    """UART programmer for FPGC."""

    def __init__(
        self,
        port: str,
        baudrate: int,
        timeout: Optional[float] = None,
        reset: bool = False,
    ):
        """Initialize the UART programmer.

        Args:
            port: Serial port path
            baudrate: Communication baudrate
            timeout: Serial timeout in seconds
            reset: Whether to reset FPGC via magic sequence on connect
        """
        self.port_path = port
        self.baudrate = baudrate
        self.timeout = timeout
        self.serial_port: Optional[serial.Serial] = None
        self.reset = reset

    def __enter__(self):
        """Context manager entry."""
        try:
            # If using rfc2217, use serial_for_url
            if self.port_path.startswith("rfc2217://"):
                # Note: I could not find a proper fix, so you have to manually disable
                #  all the purge calls for rfc2217 in the serial library.
                self.serial_port = serial.serial_for_url(
                    self.port_path, baudrate=self.baudrate, timeout=self.timeout
                )
            else:
                self.serial_port = serial.Serial(
                    self.port_path, baudrate=self.baudrate, timeout=self.timeout
                )

            logging.info(f"Connected to {self.port_path} at {self.baudrate} baud")

            if self.reset:
                logging.info("Resetting FPGC via magic sequence")
                # Send magic reset sequence
                magic_sequence = bytes.fromhex(
                    "5C6A7408D53522204F5BE72AFC0F9FCE119BE20DAB4E910E61D73E1F0F99F684"
                )
                self.serial_port.write(magic_sequence)
                sleep(0.1)  # Give FPGC time to reset
                logging.info("FPGC reset complete")
            return self
        except SerialException as e:
            raise UARTProgrammerError(
                f"Failed to open serial port {self.port_path}: {e}"
            )

    def __exit__(self, exc_type, exc_val, exc_tb):
        """Context manager exit."""
        if self.serial_port and self.serial_port.is_open:
            self.serial_port.close()
            logging.info("Serial port closed")

    def read_binary_file(self, file_path: Path) -> bytearray:
        """Read binary file and return as bytearray.

        Args:
            file_path: Path to binary file

        Returns:
            Binary data as bytearray

        Raises:
            UARTProgrammerError: If file cannot be read
        """
        try:
            with open(file_path, "rb") as f:
                data = bytearray(f.read())
            logging.info(f"Read {len(data)} bytes from {file_path}")
            return data
        except IOError as e:
            raise UARTProgrammerError(f"Failed to read file {file_path}: {e}")

    def create_word_list(self, data: bytearray) -> list[bytearray]:
        """Convert bytearray to list of 4-byte words.

        Args:
            data: Input binary data

        Returns:
            List of 4-byte words
        """
        word_size = 4
        word_list = [
            data[i * word_size : (i + 1) * word_size]
            for i in range((len(data) + word_size - 1) // word_size)
        ]

        # Pad last word if necessary
        if word_list and len(word_list[-1]) < word_size:
            word_list[-1].extend(bytes(word_size - len(word_list[-1])))

        logging.info(f"Created {len(word_list)} words from data")
        return word_list

    def send_program(self, file_path: Path, test_mode: bool = False) -> int:
        """Program FPGC with binary file.

        Args:
            file_path: Path to binary file
            test_mode: Whether to run in test mode

        Returns:
            Exit code (0 for success, or FPGC return value in test mode)

        Raises:
            UARTProgrammerError: If programming fails
        """
        if not self.serial_port:
            raise UARTProgrammerError("Serial port not initialized")

        # Give FPGC time to reset
        sleep(0.5)

        # Read and parse binary file
        data = self.read_binary_file(file_path)
        word_list = self.create_word_list(data)

        if len(word_list) < 3:
            raise UARTProgrammerError("Binary file too small - needs at least 3 words")

        # Get file size from address 2 (third word)
        file_size_bytes = bytes(word_list[2])
        file_size = int.from_bytes(file_size_bytes, "big")

        logging.info(f"Program size: {file_size} words")

        if file_size < 3:
            raise UARTProgrammerError("Program size too small - needs at least 3 words")

        try:
            # Send file size
            for byte in file_size_bytes:
                self.serial_port.write(bytes([byte]))
                # sleep(0.0001)

            # Send all words
            for word_index in range(file_size):
                for byte in word_list[word_index]:
                    self.serial_port.write(bytes([byte]))
                    # sleep(0.0001)

            logging.info("Finished sending program data")

            # Wait for completion signal
            completion_signal = self.serial_port.read(1)
            if len(completion_signal) != 1:
                raise UARTProgrammerError("Failed to receive completion signal")

            logging.info(f"Program sent successfully: {completion_signal}")

            if test_mode:
                # Read return code from FPGC
                return_code_bytes = self.serial_port.read(1)
                if len(return_code_bytes) != 1:
                    raise UARTProgrammerError("Failed to receive test mode return code")

                return_code = int.from_bytes(return_code_bytes, "little")
                logging.info(f"FPGC returned: {return_code}")
                return return_code

        except SerialException as e:
            raise UARTProgrammerError(f"Serial communication error: {e}")

        return 0

    def monitor_serial(self, duration: int = 0):
        """Continuously read from the serial port and print received bytes to the terminal.

        Args:
            duration: Duration in seconds to monitor (0 for indefinite)
        """
        if not self.serial_port:
            raise UARTProgrammerError("Serial port not initialized")

        if duration > 0:
            print(
                f"--- Serial monitor started for {duration} seconds. Press Ctrl+C to exit early. ---"
            )
        else:
            print("--- Serial monitor started. Press Ctrl+C to exit. ---")

        # Save original timeout and set a short timeout for non-blocking behavior
        original_timeout = self.serial_port.timeout
        self.serial_port.timeout = 0.1  # 100ms timeout

        start_time = time.time()
        try:
            while True:
                # Check if duration has elapsed
                if duration > 0 and (time.time() - start_time) >= duration:
                    print("\n--- Serial monitor duration expired. ---")
                    break

                data = self.serial_port.read(1)
                if data:
                    # Print received byte as character
                    print(data.decode(errors="ignore"), end="", flush=True)
        except KeyboardInterrupt:
            print("\n--- Serial monitor stopped. ---")
        except SerialException as e:
            raise UARTProgrammerError(
                f"Serial communication error during monitoring: {e}"
            )
        finally:
            # Restore original timeout
            self.serial_port.timeout = original_timeout


def setup_logging(verbose: bool = False) -> None:
    """Setup logging configuration."""
    level = logging.DEBUG if verbose else logging.INFO
    logging.basicConfig(
        level=level,
        format="%(asctime)s - %(levelname)s - %(message)s",
        handlers=[logging.StreamHandler(sys.stderr)],
    )


def parse_arguments() -> argparse.Namespace:
    """Parse command line arguments."""
    parser = argparse.ArgumentParser(
        description="UART programmer tool for FPGC",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )

    parser.add_argument(
        "-f",
        "--file",
        type=Path,
        default=Path("Software/ASM/Output/code.bin"),
        help="Path to binary file to program",
    )

    parser.add_argument(
        "-p", "--port", type=str, default="/dev/ttyUSB0", help="Serial port path"
    )

    parser.add_argument(
        "-b",
        "--baudrate",
        type=int,
        default=1000000,
        help="Serial communication baudrate",
    )

    parser.add_argument(
        "--test-mode",
        action="store_true",
        help="Enable test mode (wait for return code from FPGC)",
    )

    parser.add_argument(
        "-v", "--verbose", action="store_true", help="Enable verbose logging"
    )
    parser.add_argument(
        "-m",
        "--monitor",
        action="store_true",
        help="After programming, monitor and print serial output",
    )

    parser.add_argument(
        "--monitor-duration",
        type=int,
        default=0,
        help="Duration in seconds to monitor serial output after programming (0 for indefinite)",
    )

    parser.add_argument(
        "-r",
        "--reset",
        action="store_true",
        help="Reset FPGC via magic sequence before programming",
    )

    return parser.parse_args()


def main() -> int:
    """Main entry point."""
    args = parse_arguments()
    setup_logging(args.verbose)

    # Validate file path
    if not args.file.exists():
        logging.error(f"Binary file not found: {args.file}")
        return 1

    if not args.file.is_file():
        logging.error(f"Path is not a file: {args.file}")
        return 1

    try:
        with UARTProgrammer(args.port, args.baudrate, reset=args.reset) as programmer:
            result = programmer.send_program(args.file, args.test_mode)
            if args.monitor:
                programmer.monitor_serial(args.monitor_duration)
            return result
    except UARTProgrammerError as e:
        logging.error(f"Programmer error: {e}")
        return 1
    except KeyboardInterrupt:
        logging.info("Operation interrupted by user")
        return 1
    except Exception as e:
        logging.error(f"Unexpected error: {e}")
        return 1


if __name__ == "__main__":
    sys.exit(main())
