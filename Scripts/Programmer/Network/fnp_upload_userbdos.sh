#!/bin/bash
# Compile a userBDOS C program and upload it to the FPGC /bin directory via FNP.
# Uses B32CC with -user-bdos flag and ASMPY with -h -i (header + position independent).
# Auto-detects the Ethernet interface.
#
# Usage: ./fnp_upload_userbdos.sh <c_filename_in_userBDOS_dir_without_extension>
# Example: ./fnp_upload_userbdos.sh hello

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
FNP_TOOL="$SCRIPT_DIR/fnp_tool.py"

if [ $# -ne 1 ]; then
    echo "Usage: $0 <c_filename_in_userBDOS_dir_without_extension>"
    echo "Example: $0 hello"
    echo ""
    echo "Available programs:"
    find "$PROJECT_ROOT/Software/C/userBDOS" -name "*.c" -type f 2>/dev/null | \
        sed "s|$PROJECT_ROOT/Software/C/userBDOS/||" | sed 's|.c||' | sort
    exit 1
fi

C_FILENAME="$1"
C_FILENAME_WITHOUT_DIR="${C_FILENAME##*/}"
C_SOURCE="Software/C/userBDOS/${C_FILENAME}.c"
ASM_OUTPUT="Software/ASM/Output/${C_FILENAME_WITHOUT_DIR}.asm"
LIST_OUTPUT="Software/ASM/Output/code.list"
BIN_OUTPUT="Software/ASM/Output/code.bin"
B32CC="BuildTools/B32CC/output/b32cc"

cd "$PROJECT_ROOT"

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

# Step 1: Compile C to assembly using B32CC with -user-bdos flag
echo "Compiling C to assembly (userBDOS mode)..."
cd Software/C
if ../../"$B32CC" "userBDOS/${C_FILENAME}.c" "../../${ASM_OUTPUT}" "-user-bdos"
then
    echo "C compilation successful"
else
    echo "C compilation failed"
    cd ../..
    deactivate
    exit 1
fi
cd ../..

# Step 2: Assemble to binary using ASMPY with -h (header) and -i (position independent)
echo "Assembling to binary (position independent)..."
if asmpy "$ASM_OUTPUT" "$LIST_OUTPUT" -h -i
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

# Step 4: Upload to FPGC /bin directory via FNP
FPGC_PATH="/bin/${C_FILENAME_WITHOUT_DIR}.bin"
echo ""
echo "Uploading to FPGC: $FPGC_PATH"
python3 "$FNP_TOOL" upload "$BIN_OUTPUT" "$FPGC_PATH"

deactivate

echo ""
echo "Done! Program uploaded to $FPGC_PATH"
