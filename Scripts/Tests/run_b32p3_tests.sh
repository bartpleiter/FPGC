#!/bin/bash

# B32P3 CPU test runner
# This is a wrapper around run_cpu_tests.sh that uses the B32P3 testbench

# Exit immediately if a command exits with a non-zero status
set -e

# Activate the virtual environment
source .venv/bin/activate

# Parse arguments
SEQUENTIAL=""
WORKERS=""
TEST_FILE=""
MODE=""

while [[ $# -gt 0 ]]; do
    case $1 in
        --sequential)
            SEQUENTIAL="--sequential"
            shift
            ;;
        --workers)
            WORKERS="--workers $2"
            shift 2
            ;;
        --rom)
            MODE="--rom"
            shift
            ;;
        --ram)
            MODE="--ram"
            shift
            ;;
        *)
            TEST_FILE="$1"
            shift
            ;;
    esac
done

# Run the CPU tests with B32P3 testbench
if [ -n "$TEST_FILE" ]; then
    python3 Scripts/Tests/cpu_tests.py $MODE $SEQUENTIAL $WORKERS --testbench Hardware/FPGA/Verilog/Simulation/b32p3_tests_tb.v "$TEST_FILE"
else
    python3 Scripts/Tests/cpu_tests.py $MODE $SEQUENTIAL $WORKERS --testbench Hardware/FPGA/Verilog/Simulation/b32p3_tests_tb.v
fi

# Deactivate virtual environment
deactivate
