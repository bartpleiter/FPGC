#!/bin/bash

# Latency Benchmark Script for FPGC Memory System
# Compiles and runs the latency benchmark testbench

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

cd "$PROJECT_ROOT"

echo "=== FPGC Memory Latency Benchmark ==="
echo ""

# Compile the testbench
echo "Compiling testbench..."
iverilog -o Hardware/FPGA/Verilog/Simulation/Output/latency_benchmark.out \
    -I Hardware/FPGA/Verilog/Modules \
    Hardware/FPGA/Verilog/Simulation/latency_benchmark_tb.v

# Run simulation
echo "Running simulation..."
echo ""
vvp Hardware/FPGA/Verilog/Simulation/Output/latency_benchmark.out

echo ""
echo "VCD file generated: Hardware/FPGA/Verilog/Simulation/Output/latency_benchmark.vcd"
echo "Open with: gtkwave Hardware/FPGA/Verilog/Simulation/Output/latency_benchmark.vcd"
