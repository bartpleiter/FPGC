#!/usr/bin/env python3
"""
UART Flasher Tool for FPGC
A tool to flash binary files to FPGC via UART communication.
"""

import argparse
import logging
import sys
from pathlib import Path
from time import sleep
from typing import Optional

import serial
from serial.serialutil import SerialException


class UARTFlasherError(Exception):
    """Custom exception for UART flasher errors."""
    pass


class UARTFlasher:
    """UART flasher for FPGC."""
    
    def __init__(self, port: str, baudrate: int, timeout: Optional[float] = None):
        """Initialize the UART flasher.
        
        Args:
            port: Serial port path
            baudrate: Communication baudrate
            timeout: Serial timeout in seconds
        """
        self.port_path = port
        self.baudrate = baudrate
        self.timeout = timeout
        self.serial_port: Optional[serial.Serial] = None
        
    def __enter__(self):
        """Context manager entry."""
        try:
            self.serial_port = serial.Serial(
                self.port_path, 
                baudrate=self.baudrate, 
                timeout=self.timeout
            )
            logging.info(f"Connected to {self.port_path} at {self.baudrate} baud")

            logging.info("Resetting FPGC via magic sequence")
            # Send magic reset sequence
            magic_sequence = bytes.fromhex("5C6A7408D53522204F5BE72AFC0F9FCE119BE20DAB4E910E61D73E1F0F99F684")
            self.serial_port.write(magic_sequence)
            sleep(0.1)  # Give FPGC time to reset
            logging.info("FPGC reset complete")
            return self
        except SerialException as e:
            raise UARTFlasherError(f"Failed to open serial port {self.port_path}: {e}")
            
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
            UARTFlasherError: If file cannot be read
        """
        try:
            with open(file_path, "rb") as f:
                data = bytearray(f.read())
            logging.info(f"Read {len(data)} bytes from {file_path}")
            return data
        except IOError as e:
            raise UARTFlasherError(f"Failed to read file {file_path}: {e}")
            
    def create_word_list(self, data: bytearray) -> list[bytearray]:
        """Convert bytearray to list of 4-byte words.
        
        Args:
            data: Input binary data
            
        Returns:
            List of 4-byte words
        """
        word_size = 4
        word_list = [
            data[i * word_size:(i + 1) * word_size] 
            for i in range((len(data) + word_size - 1) // word_size)
        ]
        
        # Pad last word if necessary
        if word_list and len(word_list[-1]) < word_size:
            word_list[-1].extend(bytes(word_size - len(word_list[-1])))
            
        logging.info(f"Created {len(word_list)} words from data")
        return word_list
        
    def flash_program(self, file_path: Path, test_mode: bool = False) -> int:
        """Flash program to FPGC.
        
        Args:
            file_path: Path to binary file
            test_mode: Whether to run in test mode
            
        Returns:
            Exit code (0 for success, or FPGC return value in test mode)
            
        Raises:
            UARTFlasherError: If flashing fails
        """
        if not self.serial_port:
            raise UARTFlasherError("Serial port not initialized")
            
        # Give FPGC time to reset
        sleep(0.3)
        
        # Read and parse binary file
        data = self.read_binary_file(file_path)
        word_list = self.create_word_list(data)
        
        if len(word_list) < 3:
            raise UARTFlasherError("Binary file too small - needs at least 3 words")
            
        # Get file size from address 2 (third word)
        file_size_bytes = bytes(word_list[2])
        file_size = int.from_bytes(file_size_bytes, "big")
        
        logging.info(f"Program size: {file_size} words")
        
        if file_size < 3:
            raise UARTFlasherError("Program size too small - needs at least 3 words")
        
        try:
            # Send file size
            for byte in file_size_bytes:
                self.serial_port.write(bytes([byte]))
            logging.debug("Sent file size")
            
            # Read confirmation (4 bytes)
            confirmation = self.serial_port.read(4)
            if len(confirmation) != 4:
                raise UARTFlasherError("Failed to receive size confirmation")
            
            logging.info(f"Received file size confirmation: {confirmation}")
            
            # Send all words
            for word_index in range(file_size):
                for byte in word_list[word_index]:
                    self.serial_port.write(bytes([byte]))
                    
                if (word_index + 1) % 100 == 0:  # Progress indicator
                    logging.debug(f"Sent {word_index + 1}/{file_size} words")
                    
            logging.info("Finished sending program data")
            
            # Wait for completion signal
            completion_signal = self.serial_port.read(1)
            if len(completion_signal) != 1:
                raise UARTFlasherError("Failed to receive completion signal")
                
            logging.info(f"Program sent successfully: {completion_signal}")
            
            if test_mode:
                # Read return code from FPGC
                return_code_bytes = self.serial_port.read(1)
                if len(return_code_bytes) != 1:
                    raise UARTFlasherError("Failed to receive test mode return code")
                    
                return_code = int.from_bytes(return_code_bytes, "little")
                logging.info(f"FPGC returned: {return_code}")
                return return_code
                
        except SerialException as e:
            raise UARTFlasherError(f"Serial communication error: {e}")
            
        return 0

    def monitor_serial(self):
        """Continuously read from the serial port and print received bytes to the terminal."""
        if not self.serial_port:
            raise UARTFlasherError("Serial port not initialized")
        print("--- Serial monitor started. Press Ctrl+C to exit. ---")
        try:
            while True:
                data = self.serial_port.read(1)
                if data:
                    # Print as hex
                    print(f"0x{data.hex()}", end=' ', flush=True)
        except KeyboardInterrupt:
            print("\n--- Serial monitor stopped. ---")
        except SerialException as e:
            raise UARTFlasherError(f"Serial communication error during monitoring: {e}")


def setup_logging(verbose: bool = False) -> None:
    """Setup logging configuration."""
    level = logging.DEBUG if verbose else logging.INFO
    logging.basicConfig(
        level=level,
        format='%(asctime)s - %(levelname)s - %(message)s',
        handlers=[
            logging.StreamHandler(sys.stderr)
        ]
    )


def parse_arguments() -> argparse.Namespace:
    """Parse command line arguments."""
    parser = argparse.ArgumentParser(
        description="UART flasher tool for FPGC",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter
    )
    
    parser.add_argument(
        "-f", "--file",
        type=Path,
        default=Path("Software/BareMetalASM/Output/code.bin"),
        help="Path to binary file to flash"
    )
    
    parser.add_argument(
        "-p", "--port",
        type=str,
        default="/dev/ttyUSB0",
        help="Serial port path"
    )
    
    parser.add_argument(
        "-b", "--baudrate",
        type=int,
        default=1000000,
        help="Serial communication baudrate"
    )
    
    parser.add_argument(
        "--test-mode",
        action="store_true",
        help="Enable test mode (wait for return code from FPGC)"
    )
    
    parser.add_argument(
        "-v", "--verbose",
        action="store_true",
        help="Enable verbose logging"
    )
    parser.add_argument(
        "--monitor",
        action="store_true",
        help="After flashing, monitor and print serial output"
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
        with UARTFlasher(args.port, args.baudrate) as flasher:
            result = flasher.flash_program(args.file, args.test_mode)
            if args.monitor:
                flasher.monitor_serial()
            return result
    except UARTFlasherError as e:
        logging.error(f"Flasher error: {e}")
        return 1
    except KeyboardInterrupt:
        logging.info("Operation interrupted by user")
        return 1
    except Exception as e:
        logging.error(f"Unexpected error: {e}")
        return 1


if __name__ == "__main__":
    sys.exit(main())
