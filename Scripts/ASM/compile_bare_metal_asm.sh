#!/bin/bash
# Script to compile bare metal assembly code for RAM execution

# Check for required argument
if [ $# -ne 1 ]; then
    echo "Usage: $0 <asm_filename_in_Programs_dir_without_extension>"
    exit 1
fi

ASM_FILENAME="$1"

# Activate the virtual environment
source .venv/bin/activate

# Create output directory if it does not exist yet
mkdir -p Software/BareMetalASM/Output

# Compile code
echo "Compiling code"
if asmpy "Software/BareMetalASM/Programs/${ASM_FILENAME}.asm" "Software/BareMetalASM/Output/code.list" -h
then
    echo "Code compiled successfully"
else
    echo "Compilation failed"
    exit
fi

# Convert to binary
echo "Converting output to binary"
perl -ne 'print pack("B32", $_)' < Software/BareMetalASM/Output/code.list > Software/BareMetalASM/Output/code.bin
echo "Converted to binary"

# If you want to inspect the binary for debugging this script, you can use:
# xxd -b -c4 Software/BareMetalASM/Output/code.bin

# Deactivate virtual environment
deactivate
