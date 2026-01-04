#!/bin/bash
# Script to flash BDOS to SPI flash
#
# This script:
# 1. Assumes BDOS has already been compiled to binary (code.bin)
# 2. Converts the binary to flash_binary.c format
# 3. Compiles the flash_writer program (which embeds the binary)
# 4. Flashes the flash_writer via UART, which writes to SPI flash

set -e

# Paths
BIN_OUTPUT="Software/ASM/Output/code.bin"
FLASH_BINARY_C="Software/C/bareMetal/flash_writer/flash_binary.c"
BIN2FLASH_SCRIPT="Scripts/Programmer/bin2flash.py"
B32CC="BuildTools/B32CC/output/b32cc"


# Check if compiler exists
if [ ! -f "$B32CC" ]; then
    echo "Error: B32CC compiler not found. Run 'make b32cc' first."
    exit 1
fi

# Step 1: Convert binary to flash_binary.c
echo "Step 1: Converting binary to flash_binary.c..."
# Create FLASH_BINARY_C if it doesn't exist
if [ ! -f "$FLASH_BINARY_C" ]; then
    touch "$FLASH_BINARY_C"
fi
# Activate venv for python script
source .venv/bin/activate
python3 "$BIN2FLASH_SCRIPT" "$BIN_OUTPUT" "$FLASH_BINARY_C"
deactivate
echo ""

# Step 2: Compile the flash_writer program
echo "Step 2: Compiling flash_writer program..."
./Scripts/BCC/compile_bare_metal_c.sh flash_writer/flash_writer
echo ""

# Step 3: Run the flash_writer via UART
echo "Step 3: Running flash_writer on FPGC via UART..."
./Scripts/Programmer/UART/run_uart.sh
echo ""
