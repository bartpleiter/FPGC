#!/bin/bash
# Compile a userBDOS C program and upload it to the FPGC /bin directory via FNP.
# Uses compile_user_bdos.sh for compilation, then uploads via FNP.
# Auto-detects the Ethernet interface.
#
# Usage: ./fnp_upload_userbdos.sh <c_filename_in_userBDOS_dir_without_extension>
# Example: ./fnp_upload_userbdos.sh hello

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
FNP_TOOL="$SCRIPT_DIR/fnp_tool.py"
COMPILE_SCRIPT="$PROJECT_ROOT/Scripts/BCC/compile_user_bdos.sh"

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
BIN_OUTPUT="Software/ASM/Output/code.bin"

cd "$PROJECT_ROOT"

# Step 1: Compile using the compile script
echo "=== Compiling userBDOS program ==="
"$COMPILE_SCRIPT" "$C_FILENAME"

# Activate the virtual environment (needed for FNP tool)
source .venv/bin/activate

# Step 2: Upload to FPGC /bin directory via FNP
FPGC_PATH="/bin/${C_FILENAME_WITHOUT_DIR}"
echo ""
echo "Uploading to FPGC: $FPGC_PATH"
python3 "$FNP_TOOL" upload "$BIN_OUTPUT" "$FPGC_PATH"

deactivate

echo ""
echo "Done! Program uploaded to $FPGC_PATH"
