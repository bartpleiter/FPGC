#!/bin/bash
# Script to flash a bare metal C program to SPI flash
#
# This script:
# 1. Compiles the target C program to binary
# 2. Converts the binary to flash_binary.c format
# 3. Compiles the flash_writer program (which embeds the binary)
# 4. Flashes the flash_writer via UART, which writes to SPI flash

set -e

# Check for required argument
if [ $# -ne 1 ]; then
    echo "Usage: $0 <c_filename_in_bareMetal_dir_without_extension>"
    echo "Example: $0 libtests/test_term"
    exit 1
fi

C_FILENAME="$1"
C_FILENAME_WITHOUT_DIR="${C_FILENAME##*/}"
C_SOURCE="Software/C/bareMetal/${C_FILENAME}.c"

# Paths
BIN_OUTPUT="Software/ASM/Output/code.bin"
FLASH_BINARY_C="Software/C/bareMetal/flash_writer/flash_binary.c"
BIN2FLASH_SCRIPT="Scripts/Programmer/bin2flash.py"
B32CC="BuildTools/B32CC/output/b32cc"

# Check if source file exists
if [ ! -f "$C_SOURCE" ]; then
    echo "Error: Source file not found: $C_SOURCE"
    exit 1
fi

# Check if compiler exists
if [ ! -f "$B32CC" ]; then
    echo "Error: B32CC compiler not found. Run 'make b32cc' first."
    exit 1
fi

echo "=============================================="
echo "FPGC SPI Flash Programmer"
echo "=============================================="
echo "Target program: $C_FILENAME"
echo ""

# Step 1: Compile the target C program to binary
echo "Step 1: Compiling target program to binary..."
./Scripts/BCC/compile_bare_metal_c.sh "$C_FILENAME"
echo ""

# Step 2: Convert binary to flash_binary.c
echo "Step 2: Converting binary to flash_binary.c..."
# Create FLASH_BINARY_C if it doesn't exist
if [ ! -f "$FLASH_BINARY_C" ]; then
    touch "$FLASH_BINARY_C"
fi
# Activate venv for python script
source .venv/bin/activate
python3 "$BIN2FLASH_SCRIPT" "$BIN_OUTPUT" "$FLASH_BINARY_C"
deactivate
echo ""

# Step 3: Compile the flash_writer program
echo "Step 3: Compiling flash_writer program..."
./Scripts/BCC/compile_bare_metal_c.sh flash_writer/flash_writer
echo ""

# Step 4: Flash the flash_writer via UART
echo "Step 4: Flashing flash_writer to FPGC via UART..."
./Scripts/Programmer/UART/flash_uart.sh
echo ""

echo "=============================================="
echo "SPI Flash programming complete!"
echo "The program '$C_FILENAME' has been written to SPI flash."
echo "=============================================="
