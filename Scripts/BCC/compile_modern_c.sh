#!/bin/bash
# Script to compile C code using the modern toolchain:
#   cproc (C frontend) → QBE (backend) → ASMPY (assembler) → binary
#
# Supports mixed .c and .asm inputs. Assembly files (like crt0 startup code)
# pass through to the linker directly; C files go through cproc → QBE first.
#
# Usage:
#   # Bare metal program (with crt0 startup):
#   ./compile_modern_c.sh Software/ASM/crt0/crt0_baremetal.asm program.c -h -o output.bin
#
#   # UserBDOS program (position-independent):
#   ./compile_modern_c.sh Software/ASM/crt0/crt0_userbdos.asm program.c -h -i -o output.bin
#
#   # BDOS kernel (with syscall vector):
#   ./compile_modern_c.sh Software/ASM/crt0/crt0_bdos.asm src1.c src2.c -h -s -o output.bin
#
#   # Raw C (no startup, e.g. for testing):
#   ./compile_modern_c.sh program.c -o output.bin

set -e

# Tool paths (relative to project root)
CPROC="BuildTools/cproc/output/cproc-qbe"
QBE="BuildTools/QBE/output/qbe"

# Parse arguments
HEADER_FLAG=""
INDEPENDENT_FLAG=""
SYSCALL_FLAG=""
OUTPUT=""
INPUT_FILES=()

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
        -s|--syscall)
            SYSCALL_FLAG="-s"
            shift
            ;;
        -o|--output)
            OUTPUT="$2"
            shift 2
            ;;
        *.c|*.asm)
            INPUT_FILES+=("$1")
            shift
            ;;
        *)
            echo "Unknown argument: $1"
            echo "Usage: $0 [crt0.asm] <source.c> [source2.c ...] [-o output.bin] [-h] [-i] [-s]"
            exit 1
            ;;
    esac
done

if [ ${#INPUT_FILES[@]} -eq 0 ]; then
    echo "Usage: $0 [crt0.asm] <source.c> [source2.c ...] [-o output.bin] [-h] [-i] [-s]"
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
echo "=== Compiling sources ==="
for input_file in "${INPUT_FILES[@]}"; do
    base=$(basename "${input_file%.*}")
    if [[ "$input_file" == *.asm ]]; then
        # Assembly file — copy directly to temp dir (preserves link order)
        asm_file="$TMPDIR/${base}.asm"
        cp "$input_file" "$asm_file"
        echo "  $input_file (assembly, pass-through)"
    elif [[ "$input_file" == *.c ]]; then
        # C file — compile through cproc → QBE
        asm_file="$TMPDIR/${base}.asm"
        echo "  $input_file → $asm_file"
        "$CPROC" -t b32p3 "$input_file" 2>/dev/null | "$QBE" > "$asm_file"
    fi
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
    [ -n "$SYSCALL_FLAG" ] && ASMPY_FLAGS="$ASMPY_FLAGS -s"
    asmpy "${ASM_FILES[0]}" "$LIST_OUTPUT" $ASMPY_FLAGS
else
    # Multi-file: link then assemble
    LINKER_FLAGS=""
    [ -n "$HEADER_FLAG" ] && LINKER_FLAGS="$LINKER_FLAGS -H"
    [ -n "$INDEPENDENT_FLAG" ] && LINKER_FLAGS="$LINKER_FLAGS -i"
    [ -n "$SYSCALL_FLAG" ] && LINKER_FLAGS="$LINKER_FLAGS -s"
    python -m asmpy.linker "${ASM_FILES[@]}" "$LIST_OUTPUT" $LINKER_FLAGS
fi

echo "=== Converting to binary ==="
perl -ne 'print pack("B32", $_)' < "$LIST_OUTPUT" > "$OUTPUT"

deactivate

echo ""
echo "Compilation complete!"
echo "  Binary output: $OUTPUT ($(wc -c < "$OUTPUT") bytes)"
