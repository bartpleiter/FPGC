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

echo ""

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

echo ""

# Compile SPI Flash code
echo "Compiling SPI Flash code"
if asmpy Software/BareMetalASM/Simulation/sim_spiflash1.asm Hardware/Vivado/FPGC.srcs/simulation/memory/spiflash1_32.list -h
then
    echo "SPI Flash code compiled successfully"
    # Convert to 8 bit lines for spiflash model
    bash Scripts/Simulation/convert_to_8_bit.sh Hardware/Vivado/FPGC.srcs/simulation/memory/spiflash1_32.list Hardware/Vivado/FPGC.srcs/simulation/memory/spiflash1.list
else
    echo "SPI Flash compilation failed"
    exit
fi

echo ""

# Compile UART Program code
echo "Compiling UART Program code"
if asmpy Software/BareMetalASM/Simulation/sim_uartprog.asm Hardware/Vivado/FPGC.srcs/simulation/memory/uartprog.list -h
then
    echo "UART Program code compiled successfully"
    # Convert to 8 bit lines for UART data
    bash Scripts/Simulation/convert_to_8_bit.sh Hardware/Vivado/FPGC.srcs/simulation/memory/uartprog.list Hardware/Vivado/FPGC.srcs/simulation/memory/uartprog_8bit.list
    
    # Copy word 8-11 to the beginning of the file to start with the file size for the bootloader
    temp_file=$(mktemp)
    sed -n '9,12p' Hardware/Vivado/FPGC.srcs/simulation/memory/uartprog_8bit.list > "$temp_file"
    cat Hardware/Vivado/FPGC.srcs/simulation/memory/uartprog_8bit.list >> "$temp_file"
    mv "$temp_file" Hardware/Vivado/FPGC.srcs/simulation/memory/uartprog_8bit.list
else
    echo "UART Program compilation failed"
    exit
fi

echo ""

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
