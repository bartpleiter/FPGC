#!/bin/bash

# Activate the virtual environment
source .venv/bin/activate

# Compile RAM bootloader code
if asmpy Software/BareMetalASM/Bootloaders/UART/test_echo_ram.asm Hardware/Vivado/FPGC.srcs/simulation/memory/ram.list -h
then
    # Convert to 256 bit lines for mig7 mock
    python3 BuildTools/Utils/convert_to_256_bit.py Hardware/Vivado/FPGC.srcs/simulation/memory/ram.list Hardware/Vivado/FPGC.srcs/simulation/memory/mig7mock.list
else
    echo "RAM compilation failed"
    exit
fi

# TODO: Convert to data in ROM code
# TODO: Compile ROM code


# TMP: simulate RAM code only
# Compile ROM code
if asmpy Software/BareMetalASM/Simulation/bootloader_rom_ram.asm Hardware/Vivado/FPGC.srcs/simulation/memory/rom.list
then
    echo "ROM code compiled successfully"
else
    echo "ROM compilation failed"
    exit
fi

# Run simulation and open gtkwave (in X11 as Wayland has issues)
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
