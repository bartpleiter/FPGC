#!/bin/bash

# Activate conda environment
eval "$(conda shell.bash hook)"
conda activate FPGC

# Compile code
cp Software/BareMetalASM/Simulation/cpu.asm Software/BuildTools/ASM/code.asm
if (cd Software/BuildTools/ASM && python3 Assembler.py bdos 0x000000 > code.list)
then
    # Move to simulation directory
    mv Software/BuildTools/ASM/code.list FPGA/Data/Simulation/rom.list
else
    # print the error, which is in code.list
    (cat Software/BuildTools/ASM/code.list)
    exit
fi

# Run simulation and open gtkwave
iverilog -o FPGA/Simulation/output/cpu.out FPGA/Simulation/integration/tb_cpu.v &&\
vvp FPGA/Simulation/output/cpu.out &&\
gtkwave --dark FPGA/Simulation/output/cpu.gtkw

conda deactivate
