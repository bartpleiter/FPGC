#!/bin/bash
# Script to flash BDOS to SPI flash
#
# This script:
# 1. Assumes BDOS has already been compiled to binary (code.bin)
# 2. Converts the binary to flash_binary.c format (C array)
# 3. Compiles the flash_writer program (modern C) which embeds the binary
# 4. Flashes the flash_writer via UART, which writes to SPI flash

set -e

# Paths
BIN_OUTPUT="Software/ASM/Output/code.bin"
FLASH_BINARY_C="Software/C/bareMetal/flash_writer/flash_binary.c"
BIN2FLASH_SCRIPT="Scripts/Programmer/bin2flash.py"

# Step 1: Convert binary to flash_binary.c
echo "Step 1: Converting binary to flash_binary.c..."
if [ ! -f "$FLASH_BINARY_C" ]; then
    touch "$FLASH_BINARY_C"
fi
source .venv/bin/activate
python3 "$BIN2FLASH_SCRIPT" "$BIN_OUTPUT" "$FLASH_BINARY_C"
deactivate
echo ""

# Step 2: Compile the flash_writer program (modern C)
echo "Step 2: Compiling flash_writer program..."
./Scripts/BCC/compile_modern_c.sh \
    Software/ASM/crt0/crt0_baremetal.asm \
    Software/C/libc/sys/_exit.asm \
    Software/C/libc/string/string.c \
    Software/C/libc/stdlib/stdlib.c \
    Software/C/libc/stdlib/malloc.c \
    Software/C/libc/ctype/ctype.c \
    Software/C/libc/stdio/stdio.c \
    Software/C/libc/sys/syscalls.c \
    Software/C/libfpgc/sys/sys_asm.asm \
    Software/C/libfpgc/sys/sys.c \
    Software/C/libfpgc/io/spi.c \
    Software/C/libfpgc/io/uart.c \
    Software/C/libfpgc/io/timer.c \
    Software/C/libfpgc/io/spi_flash.c \
    Software/C/bareMetal/flash_writer/flash_writer.c \
    --libc \
    -I Software/C/libfpgc/include \
    -I Software/C/bareMetal/flash_writer \
    -h \
    -o Software/ASM/Output/code.bin
echo ""

# Step 3: Run the flash_writer via UART
echo "Step 3: Running flash_writer on FPGC via UART..."
./Scripts/Programmer/UART/run_uart.sh
echo ""
