#!/bin/bash

# Debug a single CPU test by assembling it, setting up simulation files,
# and running the CPU simulation with GTKWave

set -e

if [ -z "$1" ]; then
    echo "Usage: $0 <test_file>"
    echo "Example: $0 1_load.asm"
    echo ""
    echo "Available tests:"
    ls -1 Tests/CPU/*.asm | xargs -n1 basename
    exit 1
fi

TEST_FILE="$1"

# Paths
TEST_PATH="Tests/CPU/${TEST_FILE}"
ROM_LIST="Hardware/FPGA/Verilog/Simulation/MemoryLists/rom.list"
RAM_LIST="Hardware/FPGA/Verilog/Simulation/MemoryLists/ram.list"
MIG7MOCK_LIST="Hardware/FPGA/Verilog/Simulation/MemoryLists/mig7mock.list"
BOOTLOADER_ROM="Software/BareMetalASM/Simulation/sim_jump_to_ram.asm"

# Activate the virtual environment
source .venv/bin/activate

# Check if test file exists
if [ ! -f "$TEST_PATH" ]; then
    echo "Error: Test file not found: $TEST_PATH"
    exit 1
fi

echo "=== Step 1: Assembling ${TEST_FILE} ==="

# Assemble bootloader to ROM
asmpy "$BOOTLOADER_ROM" "$ROM_LIST"
echo "Bootloader assembled to ROM"

# Assemble test code to RAM
asmpy "$TEST_PATH" "$RAM_LIST"
echo "Test code assembled to RAM"

# Convert to 256 bit lines for mig7 mock
python3 Scripts/Simulation/convert_to_256_bit.py "$RAM_LIST" "$MIG7MOCK_LIST"
echo "Converted to 256-bit format"
echo ""

echo "=== Step 2: Running CPU simulation with GTKWave ==="
echo ""

# Deactivate before calling make (it will reactivate as needed)
deactivate

# Run simulation (this will open GTKWave)
# Note: We don't use --add-ram here because we've already assembled the RAM code above
iverilog -o Hardware/FPGA/Verilog/Simulation/Output/cpu.out Hardware/FPGA/Verilog/Simulation/cpu_tb.v && \
vvp Hardware/FPGA/Verilog/Simulation/Output/cpu.out && \
if ! pgrep -x "gtkwave" > /dev/null; then
    GDK_BACKEND=x11 gtkwave --dark Hardware/FPGA/Verilog/Simulation/GTKWave/cpu.gtkw &
else
    echo "gtkwave is already running."
fi
