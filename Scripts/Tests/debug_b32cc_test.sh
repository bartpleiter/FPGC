#!/bin/bash

# Debug a single B32CC test by compiling it, setting up simulation files,
# and running the CPU simulation with GTKWave

set -e

if [ -z "$1" ]; then
    echo "Usage: $0 <test_file>"
    echo "Example: $0 04_control_flow/if_statements.c"
    echo ""
    echo "Available tests:"
    find Tests/C -name "*.c" -type f | grep -v "old_tests" | grep -v "tmp" | sed 's|Tests/C/||' | sort
    exit 1
fi

TEST_FILE="$1"
# Convert path separators to underscores for output filename (e.g., 04_control_flow/if_statements -> 04_control_flow_if_statements)
TEST_NAME=$(echo "${TEST_FILE%.c}" | tr '/' '_')

# Paths
TEST_PATH="Tests/C/${TEST_FILE}"
TMP_DIR="Tests/C/tmp"
ASM_OUTPUT="${TMP_DIR}/${TEST_NAME}.asm"
SIM_RAM="Software/ASM/Simulation/sim_ram.asm"
SIM_ROM="Software/ASM/Simulation/sim_rom.asm"
JUMP_TO_RAM="Software/ASM/Simulation/sim_jump_to_ram.asm"
B32CC="BuildTools/B32CC/output/b32cc"

# Activate the virtual environment
source .venv/bin/activate

# Check if test file exists
if [ ! -f "$TEST_PATH" ]; then
    echo "Error: Test file not found: $TEST_PATH"
    exit 1
fi

# Check if compiler exists
if [ ! -f "$B32CC" ]; then
    echo "Error: B32CC compiler not found. Run 'make b32cc' first."
    exit 1
fi

# Create tmp directory if needed
mkdir -p "$TMP_DIR"

# Step 1: Compile C to assembly
echo "=== Step 1: Compiling ${TEST_FILE} to assembly ==="
$B32CC "$TEST_PATH" "$ASM_OUTPUT"
echo "Assembly output: $ASM_OUTPUT"
echo ""

# Step 2: Copy assembly to simulation RAM file
echo "=== Step 2: Setting up simulation files ==="
cp "$ASM_OUTPUT" "$SIM_RAM"
echo "Copied $ASM_OUTPUT -> $SIM_RAM"

# Step 3: Ensure ROM contains jump-to-RAM code
cp "$JUMP_TO_RAM" "$SIM_ROM"
echo "Copied $JUMP_TO_RAM -> $SIM_ROM"
echo ""

# Step 4: Run CPU simulation
echo "=== Step 3: Running CPU simulation ==="
echo "Running: make sim-cpu"
echo ""

# Deactivate before calling make (it will reactivate as needed)
deactivate

# Run simulation with RAM enabled
make sim-cpu
