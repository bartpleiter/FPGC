#!/bin/bash

# Script to simulate the MIG7Mock testbench using Icarus Verilog

# Get the script directory and project root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Change to project root
cd "$PROJECT_ROOT"

echo "=== MIG7Mock Simulation ==="
echo "Project root: $PROJECT_ROOT"

# Compile and run the simulation
echo "Compiling testbench..."
iverilog -o mig7mock_sim \
    -I "$PROJECT_ROOT" \
    Hardware/Vivado/FPGC.srcs/simulation/mig7mock_tb.v

if [ $? -eq 0 ]; then
    echo "Compilation successful. Running simulation..."
    vvp mig7mock_sim

    if ! pgrep -x "gtkwave" > /dev/null
    then
        GDK_BACKEND=x11 gtkwave --dark Hardware/Vivado/FPGC.srcs/simulation/gtkwave/mig7mock.gtkw &
    else
        echo "gtkwave is already running."
    fi
    
    # Clean up
    rm -f mig7mock_sim
else
    echo "Compilation failed!"
    exit 1
fi
