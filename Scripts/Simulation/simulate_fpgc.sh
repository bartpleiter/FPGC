#!/bin/bash

# Activate the virtual environment
source .venv/bin/activate

# Compile ROM code
if asmpy Software/BareMetalASM/Simulation/cpu_rom.asm Hardware/Vivado/FPGC.srcs/simulation/memory/rom.list
then
    echo "ROM code compiled successfully"
else
    echo "ROM compilation failed"
    exit
fi

# Compile RAM code
if asmpy Software/BareMetalASM/Simulation/cpu_ram.asm Hardware/Vivado/FPGC.srcs/simulation/memory/ram.list
then
    # Convert to 256 bit lines for mig7 mock
    python3 BuildTools/Utils/convert_to_256_bit.py Hardware/Vivado/FPGC.srcs/simulation/memory/ram.list Hardware/Vivado/FPGC.srcs/simulation/memory/mig7mock.list
else
    echo "RAM compilation failed"
    exit
fi

# Run simulation and open gtkwave (in X11 as Wayland has issues)
mkdir -p Hardware/Vivado/FPGC.srcs/simulation/output
iverilog -o Hardware/Vivado/FPGC.srcs/simulation/output/fpgc.out Hardware/Vivado/FPGC.srcs/simulation/FPGC_tb.v &&\
vvp Hardware/Vivado/FPGC.srcs/simulation/output/fpgc.out &&\
if ! pgrep -x "gtkwave" > /dev/null
then
    GDK_BACKEND=x11 gtkwave --dark Hardware/Vivado/FPGC.srcs/simulation/gtkwave/FPGC.gtkw &
else
    echo "gtkwave is already running."
fi

# Deactivate virtual environment
deactivate
