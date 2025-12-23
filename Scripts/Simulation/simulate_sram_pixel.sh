#!/bin/bash

# Simulation script for SRAM-based Pixel Framebuffer
# Tests the external SRAM pixel framebuffer implementation with proper 25MHz GPU clock

# Activate the virtual environment
source .venv/bin/activate

# Compile ROM code
echo "Compiling ROM code"
if asmpy Software/ASM/Simulation/sim_rom.asm Hardware/FPGA/Verilog/Simulation/MemoryLists/rom.list -o 0x7800000
then
    echo "ROM code compiled successfully"
else
    echo "ROM compilation failed"
    exit 1
fi

echo ""

# Run simulation
echo "Running SRAM pixel framebuffer simulation"
echo "This simulation uses proper 25MHz GPU clock and external SRAM model"
echo ""

iverilog -o Hardware/FPGA/Verilog/Simulation/Output/sram_pixel.out Hardware/FPGA/Verilog/Simulation/sram_pixel_tb.v &&\
vvp Hardware/FPGA/Verilog/Simulation/Output/sram_pixel.out

# Check the output
if [ -f Hardware/FPGA/Verilog/Simulation/Output/frame1.ppm ]; then
    echo ""
    echo "Frame output generated. First 20 lines of frame1.ppm:"
    head -20 Hardware/FPGA/Verilog/Simulation/Output/frame1.ppm
fi

# Deactivate virtual environment
deactivate
