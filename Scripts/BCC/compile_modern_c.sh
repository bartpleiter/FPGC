#!/bin/bash
# Script to compile C code using the modern toolchain:
#   cproc (C frontend) → QBE (backend) → ASMPY (assembler) → binary
#
# For single-file compilation (no linking):
#   ./compile_modern_c.sh <source.c> [output.bin] [-h] [-i]
#
# For multi-file compilation (with linking):
#   ./compile_modern_c.sh <source1.c> <source2.c> ... [--output output.bin] [-h] [-i]

set -e

# Tool paths (relative to project root)
CPROC="BuildTools/cproc/output/cproc-qbe"
QBE="BuildTools/QBE/output/qbe"

# Parse arguments
HEADER_FLAG=""
INDEPENDENT_FLAG=""
OUTPUT=""
C_FILES=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        -h|--header)
            HEADER_FLAG="-h"
            shift
            ;;
        -i|--independent)
            INDEPENDENT_FLAG="-i"
            shift
            ;;
        -o|--output)
            OUTPUT="$2"
            shift 2
            ;;
        *.c)
            C_FILES+=("$1")
            shift
            ;;
        *)
            echo "Unknown argument: $1"
            echo "Usage: $0 <source.c> [source2.c ...] [-o output.bin] [-h] [-i]"
            exit 1
            ;;
    esac
done

if [ ${#C_FILES[@]} -eq 0 ]; then
    echo "Usage: $0 <source.c> [source2.c ...] [-o output.bin] [-h] [-i]"
    exit 1
fi

# Default output
if [ -z "$OUTPUT" ]; then
    OUTPUT="Software/ASM/Output/code.bin"
fi

# Verify tools exist
for tool in "$CPROC" "$QBE"; do
    if [ ! -f "$tool" ]; then
        echo "Error: $tool not found. Run 'make qbe cproc' first."
        exit 1
    fi
done

# Activate virtual environment
source .venv/bin/activate

# Create temp directory for intermediate files
TMPDIR=$(mktemp -d)
trap "rm -rf $TMPDIR" EXIT

ASM_FILES=()
echo "=== Compiling C sources ==="
for c_file in "${C_FILES[@]}"; do
    base=$(basename "${c_file%.c}")
    asm_file="$TMPDIR/${base}.asm"
    echo "  $c_file → $asm_file"

    "$CPROC" -t b32p3 "$c_file" 2>/dev/null | "$QBE" > "$asm_file"
    ASM_FILES+=("$asm_file")
done

echo "=== Assembling ==="
LIST_OUTPUT="${OUTPUT%.bin}.list"
mkdir -p "$(dirname "$OUTPUT")"

if [ ${#ASM_FILES[@]} -eq 1 ]; then
    # Single file: direct assembly
    ASMPY_FLAGS=""
    [ -n "$HEADER_FLAG" ] && ASMPY_FLAGS="$ASMPY_FLAGS -h"
    [ -n "$INDEPENDENT_FLAG" ] && ASMPY_FLAGS="$ASMPY_FLAGS -i"
    asmpy "${ASM_FILES[0]}" "$LIST_OUTPUT" $ASMPY_FLAGS
else
    # Multi-file: link then assemble
    LINKER_FLAGS=""
    [ -n "$HEADER_FLAG" ] && LINKER_FLAGS="$LINKER_FLAGS -H"
    [ -n "$INDEPENDENT_FLAG" ] && LINKER_FLAGS="$LINKER_FLAGS -i"
    python -m asmpy.linker "${ASM_FILES[@]}" "$LIST_OUTPUT" $LINKER_FLAGS
fi

echo "=== Converting to binary ==="
perl -ne 'print pack("B32", $_)' < "$LIST_OUTPUT" > "$OUTPUT"

deactivate

echo ""
echo "Compilation complete!"
echo "  Binary output: $OUTPUT ($(wc -c < "$OUTPUT") bytes)"
