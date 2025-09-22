#!/bin/bash

# Activate the virtual environment
source .venv/bin/activate

# Compile ROM code
cp Software/BareMetalASM/Simulation/cpu_rom.asm BuildTools/ASM/code.asm
if (cd BuildTools/ASM && python3 Assembler.py -H > code.list)
then
    # Move to simulation directory
    mv BuildTools/ASM/code.list Hardware/Vivado/FPGC.srcs/simulation/memory/rom.list
else
    # Print the error, which is in code.list
    (cat BuildTools/ASM/code.list)
    exit
fi

# Compile RAM code
cp Software/BareMetalASM/Simulation/cpu_ram.asm BuildTools/ASM/code.asm
if (cd BuildTools/ASM && python3 Assembler.py -H > code.list)
then
    # Move to simulation directory
    mv BuildTools/ASM/code.list Hardware/Vivado/FPGC.srcs/simulation/memory/ram.list
    # Convert to 256 bit lines for mig7 mock
    python3 BuildTools/Utils/convert_to_256_bit.py Hardware/Vivado/FPGC.srcs/simulation/memory/ram.list Hardware/Vivado/FPGC.srcs/simulation/memory/mig7mock.list
else
    # Print the error, which is in code.list
    (cat BuildTools/ASM/code.list)
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
