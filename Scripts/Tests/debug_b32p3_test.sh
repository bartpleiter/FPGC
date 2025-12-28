#!/bin/bash

# Debug a single B32P3 CPU test by assembling it, setting up simulation files,
# and running the CPU simulation with GTKWave

set -e

if [ -z "$1" ]; then
    echo "Usage: $0 <test_file>"
    echo "Example: $0 09_pipeline_hazards/data_hazards_alu.asm"
    echo ""
    echo "Available tests:"
    find Tests/CPU -name "*.asm" -type f | grep -v "tmp" | sed 's|Tests/CPU/||' | sort
    exit 1
fi

TEST_FILE="$1"

# Paths
TEST_PATH="Tests/CPU/${TEST_FILE}"
ROM_LIST="Hardware/FPGA/Verilog/Simulation/MemoryLists/rom.list"
TESTBENCH="Hardware/FPGA/Verilog/Simulation/b32p3_tests_tb.v"
OUTPUT_DIR="Hardware/FPGA/Verilog/Simulation/Output"
VCD_FILE="${OUTPUT_DIR}/b32p3_test.vcd"

# Activate the virtual environment
source .venv/bin/activate

# Check if test file exists
if [ ! -f "$TEST_PATH" ]; then
    echo "Error: Test file not found: $TEST_PATH"
    exit 1
fi

echo "=== Step 1: Assembling ${TEST_FILE} ==="

# Assemble test code directly to ROM (B32P3 boots from ROM)
asmpy "$TEST_PATH" "$ROM_LIST"
echo "Test code assembled to ROM"
echo ""

echo "=== Step 2: Compiling B32P3 testbench ==="

# Compile testbench
iverilog -g2012 -o "${OUTPUT_DIR}/b32p3_debug.out" -s b32p3_tb -I . "$TESTBENCH"
echo "Testbench compiled"
echo ""

echo "=== Step 3: Running simulation ==="

# Run simulation (generates VCD file)
vvp "${OUTPUT_DIR}/b32p3_debug.out"
echo ""

echo "=== Step 4: Opening GTKWave ==="

# Check if GTKWave config exists for B32P3
GTKW_FILE="Hardware/FPGA/Verilog/Simulation/GTKWave/b32p3.gtkw"
if [ ! -f "$GTKW_FILE" ]; then
    echo "Note: No GTKWave save file found at $GTKW_FILE"
    echo "Opening VCD file directly..."
    if ! pgrep -x "gtkwave" > /dev/null; then
        GDK_BACKEND=x11 gtkwave --dark "$VCD_FILE" &
    else
        echo "gtkwave is already running."
    fi
else
    if ! pgrep -x "gtkwave" > /dev/null; then
        GDK_BACKEND=x11 gtkwave --dark "$GTKW_FILE" &
    else
        echo "gtkwave is already running."
    fi
fi

echo ""
echo "Done! GTKWave should be opening with the simulation results."
