#!/bin/bash

# Activate the virtual environment
source .venv/bin/activate

# Compile ROM code
echo "Compiling ROM code"
if asmpy Software/BareMetalASM/Simulation/sim_rom.asm Hardware/Vivado/FPGC.srcs/simulation/memory/rom.list -o 0x7800000
then
    echo "ROM code compiled successfully"
else
    echo "ROM compilation failed"
    exit
fi

# Compile RAM code
echo "Compiling RAM code"
if asmpy Software/BareMetalASM/Simulation/sim_ram.asm Hardware/Vivado/FPGC.srcs/simulation/memory/ram.list -h
then
    echo "RAM code compiled successfully"
    # Convert to 256 bit lines for mig7 mock
    python3 Scripts/Simulation/convert_to_256_bit.py Hardware/Vivado/FPGC.srcs/simulation/memory/ram.list Hardware/Vivado/FPGC.srcs/simulation/memory/mig7mock.list
else
    echo "RAM compilation failed"
    exit
fi

# Run simulation and open gtkwave (in X11 as Wayland has issues)
echo "Running simulation"
mkdir -p Hardware/Vivado/FPGC.srcs/simulation/output
iverilog -o Hardware/Vivado/FPGC.srcs/simulation/output/cpu.out Hardware/Vivado/FPGC.srcs/simulation/cpu_tb.v &&\
vvp Hardware/Vivado/FPGC.srcs/simulation/output/cpu.out &&\
if ! pgrep -x "gtkwave" > /dev/null
then
    GDK_BACKEND=x11 gtkwave --dark Hardware/Vivado/FPGC.srcs/simulation/gtkwave/cpu.gtkw &
else
    echo "gtkwave is already running."
fi

# Deactivate virtual environment
deactivate
