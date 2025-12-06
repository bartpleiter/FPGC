#!/bin/bash

arg="$1"

# Activate the virtual environment
source .venv/bin/activate

# Compile UART bootloader for RAM
echo "Compiling UART bootloader for RAM"
if asmpy Software/ASM/Bootloaders/uart_bootloader_ram_only.asm Hardware/FPGA/Verilog/Simulation/MemoryLists/ram.list -h -o 0x3FF000
then
    echo "UART bootloader code compiled successfully"
else
    echo "UART bootloader compilation failed"
    exit
fi

echo ""

# Replace the first word of ram.list with 32 1s (in hex: FFFFFFFF)
sed -i '1s/.*/11111111111111111111111111111111/' Hardware/FPGA/Verilog/Simulation/MemoryLists/ram.list

# Convert the ram.list into assembly code with .dw 0b format in Software/ASM/Bootloaders/uart_bootloader_ram_compiled.asm using UART_Bootloader_RAM_code: as label
echo "Converting RAM list to assembly format"
{
    echo "; To be used as import"
    echo ""
    echo "UART_Bootloader_RAM_code:"
    while IFS= read -r line; do
        # Skip empty lines and comments
        if [[ -n "$line" && ! "$line" =~ ^[[:space:]]*$ ]]; then
            # Remove comments (everything after //)
            clean_line=$(echo "$line" | sed 's|//.*||' | tr -d '[:space:]')
            # Only process lines that contain binary data (32 characters of 0s and 1s)
            if [[ "$clean_line" =~ ^[01]{32}$ ]]; then
                echo "    .dw 0b$clean_line"
            fi
        fi
    done < Hardware/FPGA/Verilog/Simulation/MemoryLists/ram.list
} > Software/ASM/Bootloaders/uart_bootloader_ram_compiled.asm
echo "RAM list converted to assembly format successfully"

echo ""

# Compile the resulting ROM bootloader code
echo "Compiling complete ROM bootloader"
if asmpy Software/ASM/Bootloaders/uart_bootloader_rom_ram.asm Hardware/FPGA/Verilog/Simulation/MemoryLists/rom.list -o 0x7800000
then
    echo "Complete ROM bootloader code compiled successfully"
else
    echo "Complete ROM bootloader compilation failed"
    exit
fi

# Copy rom.list to rom_bootloader.list for use in FPGA implementation
cp Hardware/FPGA/Verilog/Simulation/MemoryLists/rom.list Hardware/FPGA/Verilog/MemoryLists/rom_bootloader.list

# Compile UART Program code if requested
if [ "$arg" == "--simulate" ]; then
    echo ""
    echo "Compiling UART Program code"
    if asmpy Software/ASM/Simulation/sim_uartprog.asm Hardware/FPGA/Verilog/Simulation/MemoryLists/uartprog.list -h
    then
        echo "UART Program code compiled successfully"
        # Convert to 8 bit lines for UART data
        bash Scripts/Simulation/convert_to_8_bit.sh Hardware/FPGA/Verilog/Simulation/MemoryLists/uartprog.list Hardware/FPGA/Verilog/Simulation/MemoryLists/uartprog_8bit.list
        
        # Copy word 8-11 to the beginning of the file to start with the file size for the bootloader
        temp_file=$(mktemp)
        sed -n '9,12p' Hardware/FPGA/Verilog/Simulation/MemoryLists/uartprog_8bit.list > "$temp_file"
        cat Hardware/FPGA/Verilog/Simulation/MemoryLists/uartprog_8bit.list >> "$temp_file"
        mv "$temp_file" Hardware/FPGA/Verilog/Simulation/MemoryLists/uartprog_8bit.list
    else
        echo "UART Program compilation failed"
        exit
    fi
fi

if [ "$arg" == "--simulate" ]; then
    echo ""
    # Run simulation and open gtkwave (in X11 as Wayland has issues)
    echo "Running simulation"
    mkdir -p Hardware/FPGA/Verilog/Simulation/Output

    iverilog -Duart_simulation -o Hardware/FPGA/Verilog/Simulation/Output/cpu.out Hardware/FPGA/Verilog/Simulation/cpu_tb.v &&\
    vvp Hardware/FPGA/Verilog/Simulation/Output/cpu.out &&\
    if ! pgrep -x "gtkwave" > /dev/null
    then
        GDK_BACKEND=x11 gtkwave --dark Hardware/FPGA/Verilog/Simulation/GTKWave/cpu.gtkw &
    else
        echo "gtkwave is already running."
    fi
fi

# Deactivate virtual environment
deactivate
