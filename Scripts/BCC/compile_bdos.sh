#!/bin/bash
# Script to compile BDOS into a binary to run on the FPGC
# Uses B32CC to compile C to assembly, then ASMPY to assemble to binary


C_FILENAME="main"
C_SOURCE="Software/C/BDOS/${C_FILENAME}.c"
ASM_OUTPUT="Software/ASM/Output/bdos.asm"
LIST_OUTPUT="Software/ASM/Output/code.list"
BIN_OUTPUT="Software/ASM/Output/code.bin"

B32CC="BuildTools/B32CC/output/b32cc"

# Check if source file exists
if [ ! -f "$C_SOURCE" ]; then
    echo "Error: Source file not found: $C_SOURCE"
    exit 1
fi

# Check if compiler exists
if [ ! -f "$B32CC" ]; then
    echo "Error: B32CC compiler not found. Run 'make b32cc' first."
    exit 1
fi

# Activate the virtual environment
source .venv/bin/activate

# Create output directory if it does not exist yet
mkdir -p Software/ASM/Output

# Step 1: Compile C to assembly using B32CC
echo "Compiling C to assembly..."
# Change to Software/C directory so includes work correctly
cd Software/C
if ../../"$B32CC" "BDOS/${C_FILENAME}.c" "../../${ASM_OUTPUT}" "-bdos"
then
    echo "C compilation successful"
else
    echo "C compilation failed"
    cd ../..
    deactivate
    exit 1
fi
cd ../..

# Step 2: Assemble to binary using ASMPY
echo "Assembling to binary..."
if asmpy "$ASM_OUTPUT" "$LIST_OUTPUT" -h
then
    echo "Assembly successful"
else
    echo "Assembly failed"
    deactivate
    exit 1
fi

# Step 3: Convert to binary
echo "Converting to binary..."
perl -ne 'print pack("B32", $_)' < "$LIST_OUTPUT" > "$BIN_OUTPUT"
echo "Binary created: $BIN_OUTPUT"

# Deactivate virtual environment
deactivate

echo ""
echo "Compilation complete!"
echo "  Assembly output: $ASM_OUTPUT"
echo "  Binary output: $BIN_OUTPUT"
