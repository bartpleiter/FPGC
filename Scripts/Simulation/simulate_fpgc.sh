#!/bin/bash

# TODO: Swap conda with poetry venv

# Activate conda environment
eval "$(conda shell.bash hook)"
conda activate FPGC

# Compile code
cp Software/BareMetalASM/Simulation/cpu.asm BuildTools/ASM/code.asm
if (cd BuildTools/ASM && python3 Assembler.py -H > code.list)
then
    # Move to simulation directory
    mv BuildTools/ASM/code.list Hardware/Vivado/FPGC.srcs/simulation/memory/rom.list
else
    # Print the error, which is in code.list
    (cat BuildTools/ASM/code.list)
    exit
fi

# Run simulation and open gtkwave (in X11 as Wayland has issues)
iverilog -o Hardware/Vivado/FPGC.srcs/simulation/output/fpgc.out Hardware/Vivado/FPGC.srcs/simulation/FPGC_tb.v &&\
vvp Hardware/Vivado/FPGC.srcs/simulation/output/fpgc.out &&\
if ! pgrep -x "gtkwave" > /dev/null
then
    GDK_BACKEND=x11 gtkwave --dark Hardware/Vivado/FPGC.srcs/simulation/gtkwave/FPGC.gtkw &
else
    echo "gtkwave is already running."
fi

conda deactivate
