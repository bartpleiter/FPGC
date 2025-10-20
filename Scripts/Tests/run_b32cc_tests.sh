#!/bin/bash

# Activate the virtual environment
source .venv/bin/activate

# Compile ROM code to jump to RAM directly
if ! asmpy Software/BareMetalASM/Simulation/sim_jump_to_ram.asm Hardware/Vivado/FPGC.srcs/simulation/memory/rom.list -o 0x7800000
then
    echo "ROM compilation failed"
    exit
fi

#################################
# For now hardcoded a single test
#################################

# Compile the c test file to assembly
./BuildTools/B32CC/output/b32cc Tests/C/1_return.c Tests/C/tmp/1_return.asm

# Assemble the assembly file to the RAM initialization file
if asmpy Tests/C/tmp/1_return.asm Hardware/Vivado/FPGC.srcs/simulation/memory/ram.list -h
then
    # Convert to 256 bit lines for mig7 mock
    python3 Scripts/Simulation/convert_to_256_bit.py Hardware/Vivado/FPGC.srcs/simulation/memory/ram.list Hardware/Vivado/FPGC.srcs/simulation/memory/mig7mock.list
else
    echo "RAM compilation failed"
    exit
fi

iverilog -o Hardware/Vivado/FPGC.srcs/simulation/output/cpu.out Hardware/Vivado/FPGC.srcs/simulation/cpu_tests_tb.v
vvp Hardware/Vivado/FPGC.srcs/simulation/output/cpu.out

# Deactivate virtual environment
deactivate
